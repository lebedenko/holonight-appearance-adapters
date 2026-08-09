#include <gio/gio.h>

#include "holonight/appearance_contract.h"
#include "holonight/qt_bridge.h"
#include "holonight/tier1_projection.h"

#include "holonight/appearance.h"
#include "semanticappearance.h"

#include <holonight/config/store.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cstdint>
#include <optional>

namespace {

using Holonight::Adapters::Snapshot;
using Holonight::Adapters::Tier1Projection;

constexpr int kProtocolVersion = 1;

struct Output {
  QString name;
  QString status;
  QString mode;
  QString diagnostic;
};

struct Undo {
  enum class Kind : std::uint8_t { Gtk, GSettings } kind;
  QString target;
  QString key;
  std::optional<QString> value;
};

QString configHome() {
  const QString value = qEnvironmentVariable("XDG_CONFIG_HOME");
  return value.isEmpty() ? QDir::homePath() + QStringLiteral("/.config") : value;
}

QString statePath() {
  QString value = qEnvironmentVariable("XDG_STATE_HOME");
  if (value.isEmpty())
    value = QDir::homePath() + QStringLiteral("/.local/state");
  return value + QStringLiteral("/holonight/appearance-adapters.json");
}

QJsonObject outputJson(const Output &output) {
  QJsonObject value{{QStringLiteral("name"), output.name},
                    {QStringLiteral("status"), output.status},
                    {QStringLiteral("apply_mode"), output.mode}};
  if (!output.diagnostic.isEmpty())
    value[QStringLiteral("diagnostic")] = output.diagnostic;
  return value;
}

int respond(QString operation, const QList<Output> &outputs, bool hard_error = false) {
  bool degraded = false;
  QJsonArray encoded;
  for (const Output &output : outputs) {
    encoded.append(outputJson(output));
    degraded = degraded || output.status == QStringLiteral("unavailable") ||
               output.status == QStringLiteral("delegated") || output.status == QStringLiteral("application-owned") ||
               output.status == QStringLiteral("conflict");
  }
  const QString result = hard_error ? QStringLiteral("error")
                         : degraded ? QStringLiteral("degraded")
                                    : QStringLiteral("success");
  const QJsonObject root{{QStringLiteral("protocol_version"), kProtocolVersion},
                         {QStringLiteral("operation"), operation},
                         {QStringLiteral("result"), result},
                         {QStringLiteral("success"), !hard_error},
                         {QStringLiteral("degraded"), degraded},
                         {QStringLiteral("outputs"), encoded}};
  QFile standard_output;
  if (!standard_output.open(stdout, QIODevice::WriteOnly))
    return 1;
  standard_output.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  standard_output.write("\n");
  return hard_error ? 1 : 0;
}

std::optional<QJsonObject> readJsonObject(const QString &path) {
  QFile file(path);
  if (!file.exists())
    return QJsonObject{};
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return std::nullopt;
  return document.object();
}

bool writeJsonObject(const QString &path, const QJsonObject &object) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    return false;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  if (file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0)
    return false;
  return file.commit();
}

std::optional<QString> iniValue(const QString &path, const QString &key) { // NOLINT
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;
  bool settings = false;
  for (const QByteArray &raw : file.readAll().split('\n')) {
    const QString line = QString::fromUtf8(raw);
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
      settings = trimmed == QStringLiteral("[Settings]");
    } else if (settings) {
      const qsizetype equals = line.indexOf(u'=');
      if (equals >= 0 && line.left(equals).trimmed() == key)
        return line.mid(equals + 1).trimmed();
    }
  }
  return std::nullopt;
}

bool setIniValue(const QString &path, const QString &key, const std::optional<QString> &value) { // NOLINT
  QFile input(path);
  QByteArray contents;
  QFileDevice::Permissions permissions{};
  if (input.exists()) {
    permissions = input.permissions();
    if (!input.open(QIODevice::ReadOnly))
      return false;
    contents = input.readAll();
  }
  QList<QByteArray> lines = contents.split('\n');
  bool settings = false;
  bool section_found = false;
  bool changed = false;
  bool key_found = false;
  qsizetype insert_at = lines.size();
  for (qsizetype index = 0; index < lines.size(); ++index) {
    const QString line = QString::fromUtf8(lines[index]);
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
      if (settings && insert_at == lines.size())
        insert_at = index;
      settings = trimmed == QStringLiteral("[Settings]");
      section_found = section_found || settings;
      continue;
    }
    if (!settings)
      continue;
    const qsizetype equals = line.indexOf(u'=');
    if (equals < 0 || line.left(equals).trimmed() != key)
      continue;
    key_found = true;
    if (value) {
      const QByteArray replacement = (key + u'=' + *value).toUtf8();
      changed = lines[index] != replacement;
      lines[index] = replacement;
    } else {
      lines.removeAt(index);
      changed = true;
    }
    break;
  }
  if (!key_found && value) {
    if (!section_found) {
      if (!lines.isEmpty() && !lines.last().isEmpty())
        lines.append(QByteArray{});
      lines.append("[Settings]");
      insert_at = lines.size();
    }
    lines.insert(insert_at, (key + u'=' + *value).toUtf8());
    changed = true;
  }
  if (!changed)
    return true;
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    return false;
  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly))
    return false;
  if (permissions != QFileDevice::Permissions{})
    output.setPermissions(permissions);
  QByteArray updated = lines.join('\n');
  if (!updated.endsWith('\n'))
    updated.append('\n');
  if (output.write(updated) != updated.size())
    return false;
  return output.commit();
}

std::optional<QString> schemaKey(const QString &key, GSettingsSchema **schema_out) {
  const qsizetype slash = key.indexOf(u'/');
  if (slash < 1)
    return std::nullopt;
  GSettingsSchemaSource *source = g_settings_schema_source_get_default();
  if (source == nullptr)
    return std::nullopt;
  const QByteArray schema_name = key.left(slash).toUtf8();
  GSettingsSchema *schema = g_settings_schema_source_lookup(source, schema_name.constData(), TRUE);
  if (schema == nullptr)
    return std::nullopt;
  const QByteArray member = key.mid(slash + 1).toUtf8();
  if (!g_settings_schema_has_key(schema, member.constData())) {
    g_settings_schema_unref(schema);
    return std::nullopt;
  }
  *schema_out = schema;
  return QString::fromUtf8(member);
}

std::optional<QString> getGSetting(const QString &target) {
  GSettingsSchema *schema = nullptr;
  const auto key = schemaKey(target, &schema);
  if (!key)
    return std::nullopt;
  GSettings *settings = g_settings_new_full(schema, nullptr, nullptr);
  GVariant *value = g_settings_get_value(settings, key->toUtf8().constData());
  gchar *text = g_variant_print(value, FALSE);
  QString result = QString::fromUtf8(text);
  g_free(text);
  g_variant_unref(value);
  g_object_unref(settings);
  g_settings_schema_unref(schema);
  if (result.size() >= 2 && result.front() == u'\'' && result.back() == u'\'')
    result = result.mid(1, result.size() - 2);
  return result;
}

bool setGSetting(const QString &target, const QString &value) { // NOLINT
  GSettingsSchema *schema = nullptr;
  const auto key = schemaKey(target, &schema);
  if (!key)
    return false;
  GSettings *settings = g_settings_new_full(schema, nullptr, nullptr);
  const bool ok = g_settings_set_string(settings, key->toUtf8().constData(), value.toUtf8().constData());
  g_settings_sync();
  g_object_unref(settings);
  g_settings_schema_unref(schema);
  return ok;
}

std::optional<Snapshot> loadSnapshot(const QString &path, QString *diagnostic) {
  const auto loaded = HoloNight::Config::load(path.toStdString());
  if (!loaded) {
    *diagnostic = QStringLiteral("canonical appearance is invalid");
    return std::nullopt;
  }
  const auto resolved = Holonight::resolveAppearance(loaded.value.value().appearance); // NOLINT
  if (!resolved) {
    *diagnostic = QStringLiteral("canonical appearance cannot be resolved");
    return std::nullopt;
  }
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  const QByteArray wire = Holonight::Adapters::serializeSemanticAppearance(
      Holonight::resolveSemanticAppearance(resolved.value.value())); // NOLINT
  const auto parsed = Holonight::Adapters::parseSemanticAppearance(wire.toStdString());
  if (!parsed) {
    *diagnostic = QStringLiteral("semantic appearance v1 validation failed");
    return std::nullopt;
  }
  return parsed.value;
}

QJsonObject stateEntry(const std::optional<QString> &original, const QString &last, const QString &kind,
                       const QString &target, const QString &key = {}) { // NOLINT
  QJsonObject entry{{QStringLiteral("last"), last}, {QStringLiteral("kind"), kind}, {QStringLiteral("target"), target}};
  entry[QStringLiteral("original")] = original ? QJsonValue(*original) : QJsonValue(QJsonValue::Null);
  if (!key.isEmpty())
    entry[QStringLiteral("key")] = key;
  return entry;
}

std::optional<QString> originalValue(const QJsonObject &old, const std::optional<QString> &current) {
  if (!old.isEmpty() && old.contains(QStringLiteral("original"))) {
    const QJsonValue value = old.value(QStringLiteral("original"));
    return value.isNull() ? std::nullopt : std::optional<QString>(value.toString());
  }
  return current;
}

void restoreUndo(const QList<Undo> &undo) {
  for (auto iterator = undo.crbegin(); iterator != undo.crend(); ++iterator) {
    if (iterator->kind == Undo::Kind::Gtk)
      setIniValue(iterator->target, iterator->key, iterator->value);
    else if (iterator->value)
      setGSetting(iterator->target, *iterator->value);
  }
}

int apply(const QString &appearance) {
  QString diagnostic;
  const auto snapshot = loadSnapshot(appearance, &diagnostic);
  if (!snapshot)
    return respond(QStringLiteral("apply"),
                   {{QStringLiteral("canonical"), QStringLiteral("error"), QStringLiteral("live"), diagnostic}}, true);
  const Tier1Projection projection = Holonight::Adapters::projectTier1(*snapshot);
  const auto state_document = readJsonObject(statePath());
  if (!state_document)
    return respond(QStringLiteral("apply"),
                   {{QStringLiteral("state"), QStringLiteral("error"), QStringLiteral("live"),
                     QStringLiteral("state is corrupt or unreadable")}},
                   true);
  QJsonObject state = *state_document;
  QJsonObject entries = state.value(QStringLiteral("entries")).toObject();
  QList<Output> outputs;
  QList<Undo> undo;

  for (const auto &[target_value, projected] : projection.gsettings) {
    const QString target = QString::fromStdString(target_value);
    const QString desired = QString::fromStdString(projected);
    const auto current = getGSetting(target);
    if (!current) {
      outputs.append({target, QStringLiteral("unavailable"), QStringLiteral("live"),
                      QStringLiteral("schema or key is unavailable")});
      continue;
    }
    if (*current != desired && !setGSetting(target, desired)) {
      restoreUndo(undo);
      outputs.append({target, QStringLiteral("error"), QStringLiteral("live"),
                      QStringLiteral("writable GSettings output rejected the update")});
      return respond(QStringLiteral("apply"), outputs, true);
    }
    if (*current != desired)
      undo.append({Undo::Kind::GSettings, target, {}, current});
    const QJsonObject old = entries.value(target).toObject();
    entries[target] = stateEntry(originalValue(old, current), desired, QStringLiteral("gsettings"), target);
    outputs.append({target,
                    *current == desired ? QStringLiteral("unchanged") : QStringLiteral("applied"),
                    QStringLiteral("live"),
                    {}});
  }

  const QList<std::pair<QString, const std::map<std::string, std::string> *>> gtk{
      {configHome() + QStringLiteral("/gtk-3.0/settings.ini"), &projection.gtk3_settings},
      {configHome() + QStringLiteral("/gtk-4.0/settings.ini"), &projection.gtk4_settings}};
  for (const auto &[path, values] : gtk) {
    const QString major = path.contains(QStringLiteral("gtk-3.0")) ? QStringLiteral("gtk3") : QStringLiteral("gtk4");
    for (const auto &[key_value, projected] : *values) {
      const QString key = QString::fromStdString(key_value);
      const QString desired = QString::fromStdString(projected);
      const auto current = iniValue(path, key);
      if (!setIniValue(path, key, desired)) {
        restoreUndo(undo);
        outputs.append({major + u'/' + key, QStringLiteral("error"), QStringLiteral("relaunch"),
                        QStringLiteral("GTK settings file could not be updated atomically")});
        return respond(QStringLiteral("apply"), outputs, true);
      }
      if (current != std::optional<QString>(desired))
        undo.append({Undo::Kind::Gtk, path, key, current});
      const QString id = major + u'/' + key;
      const QJsonObject old = entries.value(id).toObject();
      entries[id] = stateEntry(originalValue(old, current), desired, QStringLiteral("gtk"), path, key);
      outputs.append(
          {id,
           current == std::optional<QString>(desired) ? QStringLiteral("unchanged") : QStringLiteral("applied"),
           QStringLiteral("relaunch"),
           {}});
    }
  }
  state[QStringLiteral("protocol_version")] = kProtocolVersion;
  state[QStringLiteral("entries")] = entries;
  if (!writeJsonObject(statePath(), state)) {
    restoreUndo(undo);
    outputs.append({QStringLiteral("state"), QStringLiteral("error"), QStringLiteral("live"),
                    QStringLiteral("adapter state could not be stored atomically")});
    return respond(QStringLiteral("apply"), outputs, true);
  }
  outputs.append({QStringLiteral("portal"), QStringLiteral("delegated"), QStringLiteral("delegated"),
                  QStringLiteral("published by the HoloNight Shell Settings portal")});
  outputs.append({QStringLiteral("xsettings"), QStringLiteral("unavailable"), QStringLiteral("delegated"),
                  QStringLiteral("no competing XSettings manager is started")});
  outputs.append({QStringLiteral("application-styling"), QStringLiteral("application-owned"),
                  QStringLiteral("relaunch"), QStringLiteral("native and libadwaita styling is preserved")});
  outputs.append({QStringLiteral("XCURSOR_THEME"), QStringLiteral("delegated"), QStringLiteral("session-restart"),
                  QStringLiteral("exported by session startup integration")});
  return respond(QStringLiteral("apply"), outputs);
}

int status() {
  const auto state = readJsonObject(statePath());
  if (!state)
    return respond(QStringLiteral("status"),
                   {{QStringLiteral("state"), QStringLiteral("error"), QStringLiteral("live"),
                     QStringLiteral("state is corrupt or unreadable")}},
                   true);
  QList<Output> outputs;
  const QJsonObject entries = state->value(QStringLiteral("entries")).toObject();
  for (auto iterator = entries.begin(); iterator != entries.end(); ++iterator) {
    const QJsonObject entry = iterator.value().toObject();
    const QString kind = entry.value(QStringLiteral("kind")).toString();
    std::optional<QString> current;
    if (kind == QStringLiteral("gtk"))
      current =
          iniValue(entry.value(QStringLiteral("target")).toString(), entry.value(QStringLiteral("key")).toString());
    else
      current = getGSetting(entry.value(QStringLiteral("target")).toString());
    const QString mode = kind == QStringLiteral("gtk") ? QStringLiteral("relaunch") : QStringLiteral("live");
    outputs.append({iterator.key(),
                    current == std::optional<QString>(entry.value(QStringLiteral("last")).toString())
                        ? QStringLiteral("applied")
                        : QStringLiteral("conflict"),
                    mode, current ? QString{} : QStringLiteral("output is absent or unavailable")});
  }
  if (outputs.isEmpty())
    outputs.append({QStringLiteral("state"), QStringLiteral("unavailable"), QStringLiteral("live"),
                    QStringLiteral("appearance has not been applied")});
  return respond(QStringLiteral("status"), outputs);
}

int revert() {
  const auto state = readJsonObject(statePath());
  if (!state)
    return respond(QStringLiteral("revert"),
                   {{QStringLiteral("state"), QStringLiteral("error"), QStringLiteral("live"),
                     QStringLiteral("state is corrupt or unreadable")}},
                   true);
  QJsonObject remaining;
  QList<Output> outputs;
  const QJsonObject entries = state->value(QStringLiteral("entries")).toObject();
  for (auto iterator = entries.begin(); iterator != entries.end(); ++iterator) {
    const QJsonObject entry = iterator.value().toObject();
    const QString kind = entry.value(QStringLiteral("kind")).toString();
    const QString target = entry.value(QStringLiteral("target")).toString();
    const QString key = entry.value(QStringLiteral("key")).toString();
    const QString last = entry.value(QStringLiteral("last")).toString();
    std::optional<QString> current = kind == QStringLiteral("gtk") ? iniValue(target, key) : getGSetting(target);
    const QString mode = kind == QStringLiteral("gtk") ? QStringLiteral("relaunch") : QStringLiteral("live");
    if (current != std::optional<QString>(last)) {
      remaining[iterator.key()] = entry;
      outputs.append({iterator.key(), QStringLiteral("conflict"), mode,
                      QStringLiteral("externally modified value was preserved")});
      continue;
    }
    const QJsonValue original_json = entry.value(QStringLiteral("original"));
    const std::optional<QString> original =
        original_json.isNull() ? std::nullopt : std::optional<QString>(original_json.toString());
    const bool ok =
        kind == QStringLiteral("gtk") ? setIniValue(target, key, original) : original && setGSetting(target, *original);
    if (!ok) {
      remaining[iterator.key()] = entry;
      outputs.append(
          {iterator.key(), QStringLiteral("error"), mode, QStringLiteral("owned output could not be restored")});
      continue;
    }
    outputs.append({iterator.key(), QStringLiteral("restored"), mode, {}});
  }
  QJsonObject updated{{QStringLiteral("protocol_version"), kProtocolVersion}, {QStringLiteral("entries"), remaining}};
  if (!writeJsonObject(statePath(), updated))
    return respond(QStringLiteral("revert"), outputs, true);
  const bool error = std::any_of(outputs.cbegin(), outputs.cend(),
                                 [](const Output &output) { return output.status == QStringLiteral("error"); });
  return respond(QStringLiteral("revert"), outputs, error);
}

} // namespace

int main(int argc, char *argv[]) { // NOLINT(bugprone-exception-escape)
  QCoreApplication application(argc, argv);
  const QStringList arguments = application.arguments();
  if (arguments.size() < 2)
    return respond(QStringLiteral("unknown"),
                   {{QStringLiteral("cli"), QStringLiteral("error"), QStringLiteral("live"),
                     QStringLiteral("an operation is required")}},
                   true);
  const QString &operation = arguments[1];
  if (operation == QStringLiteral("status"))
    return status();
  if (operation == QStringLiteral("revert"))
    return revert();
  const qsizetype appearance_index = arguments.indexOf(QStringLiteral("--appearance"));
  if (appearance_index < 0 || appearance_index + 1 >= arguments.size()) {
    return respond(operation,
                   {{QStringLiteral("cli"), QStringLiteral("error"), QStringLiteral("live"),
                     QStringLiteral("--appearance requires a path")}},
                   true);
  }
  const QString &appearance = arguments[appearance_index + 1];
  if (operation == QStringLiteral("apply"))
    return apply(appearance);
  if (operation == QStringLiteral("query")) {
    const qsizetype field_index = arguments.indexOf(QStringLiteral("--field"));
    QString diagnostic;
    const auto snapshot = loadSnapshot(appearance, &diagnostic);
    if (field_index < 0 || field_index + 1 >= arguments.size() ||
        arguments[field_index + 1] != QStringLiteral("cursor-theme") || !snapshot)
      return 1;
    QFile standard_output;
    if (!standard_output.open(stdout, QIODevice::WriteOnly))
      return 1;
    standard_output.write(QByteArray::fromStdString(snapshot->cursor_theme));
    standard_output.write("\n");
    return 0;
  }
  return respond(
      operation,
      {{QStringLiteral("cli"), QStringLiteral("error"), QStringLiteral("live"), QStringLiteral("unknown operation")}},
      true);
}
