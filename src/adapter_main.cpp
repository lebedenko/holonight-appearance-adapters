// GLib D-Bus structs contain a signals field; include them before Qt keywords.
// clang-format off
#include <gio/gio.h>
// clang-format on

#include "holonight/appearance.h"
#include "holonight/appearance_contract.h"
#include "holonight/qt_bridge.h"
#include "holonight/tier1_projection.h"
#include "labwc_theme.h"
#include "semanticappearance.h"

#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

#include <csignal>
#include <holonight/config/appearance.h>
#include <holonight/config/appearance_document.h>
#include <holonight/config/store.h>
#include <ranges>
#include <unistd.h>

#ifdef HOLONIGHT_HAVE_KCONFIG
#include <KConfig>
#include <KConfigGroup>
#include <KConfigGui>
#endif

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
  enum class Kind : std::uint8_t { Gtk, GSettings, Kde, File, LabwcFont } kind;
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
  if (value.isEmpty()) {
    value = QDir::homePath() + QStringLiteral("/.local/state");
  }
  return value + QStringLiteral("/holonight/appearance-adapters.json");
}

QJsonObject outputJson(const Output& output) {
  QJsonObject value{
      {QStringLiteral("name"), output.name},
      {QStringLiteral("status"), output.status},
      {QStringLiteral("apply_mode"), output.mode},
  };
  if (!output.diagnostic.isEmpty()) {
    value[QStringLiteral("diagnostic")] = output.diagnostic;
  }
  return value;
}

int respond(QString operation, const QList<Output>& outputs, bool hard_error = false) {
  bool degraded = false;
  QJsonArray encoded;
  for (const Output& output : outputs) {
    encoded.append(outputJson(output));
    degraded = degraded || output.status == QStringLiteral("unavailable") ||
               output.status == QStringLiteral("delegated") || output.status == QStringLiteral("application-owned") ||
               output.status == QStringLiteral("conflict");
  }
  QString result = QStringLiteral("success");
  if (hard_error) {
    result = QStringLiteral("error");
  } else if (degraded) {
    result = QStringLiteral("degraded");
  }
  const QJsonObject root{
      {QStringLiteral("protocol_version"), kProtocolVersion},
      {QStringLiteral("operation"), operation},
      {QStringLiteral("result"), result},
      {QStringLiteral("success"), !hard_error},
      {QStringLiteral("degraded"), degraded},
      {QStringLiteral("outputs"), encoded},
  };
  QFile standard_output;
  if (!standard_output.open(stdout, QIODevice::WriteOnly)) {
    return 1;
  }
  standard_output.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  standard_output.write("\n");
  return hard_error ? 1 : 0;
}

std::optional<QJsonObject> readJsonObject(const QString& path) {
  QFile file(path);
  if (!file.exists()) {
    return QJsonObject{};
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return std::nullopt;
  }
  return document.object();
}

bool writeJsonObject(const QString& path, const QJsonObject& object) {
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  if (file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0) {
    return false;
  }
  return file.commit();
}

std::optional<QString> iniValue(const QString& path, const QString& key) {  // NOLINT
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  bool settings = false;
  for (const QByteArray& raw : file.readAll().split('\n')) {
    const QString line = QString::fromUtf8(raw);
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
      settings = trimmed == QStringLiteral("[Settings]");
    } else if (settings) {
      const qsizetype equals = line.indexOf(u'=');
      if (equals >= 0 && line.left(equals).trimmed() == key) {
        return line.mid(equals + 1).trimmed();
      }
    }
  }
  return std::nullopt;
}

bool setIniValue(const QString& path, const QString& key, const std::optional<QString>& value) {  // NOLINT
  QFile input(path);
  QByteArray contents;
  QFileDevice::Permissions permissions{};
  if (input.exists()) {
    permissions = input.permissions();
    if (!input.open(QIODevice::ReadOnly)) {
      return false;
    }
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
      if (settings && insert_at == lines.size()) {
        insert_at = index;
      }
      settings = trimmed == QStringLiteral("[Settings]");
      section_found = section_found || settings;
      continue;
    }
    if (!settings) {
      continue;
    }
    const qsizetype equals = line.indexOf(u'=');
    if (equals < 0 || line.left(equals).trimmed() != key) {
      continue;
    }
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
      if (!lines.isEmpty() && !lines.last().isEmpty()) {
        lines.append(QByteArray{});
      }
      lines.append("[Settings]");
      insert_at = lines.size();
    }
    lines.insert(insert_at, (key + u'=' + *value).toUtf8());
    changed = true;
  }
  if (!changed) {
    return true;
  }
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly)) {
    return false;
  }
  if (permissions != QFileDevice::Permissions{}) {
    output.setPermissions(permissions);
  }
  QByteArray updated = lines.join('\n');
  if (!updated.endsWith('\n')) {
    updated.append('\n');
  }
  if (output.write(updated) != updated.size()) {
    return false;
  }
  return output.commit();
}

bool kdeFontKey(const QString& key) { return key == QStringLiteral("font") || key == QStringLiteral("fixed"); }

QString kdeGroup(const QString& key) {
  const qsizetype slash = key.indexOf(u'/');
  return key.left(slash);
}

QString kdeMember(const QString& key) { return key.mid(key.indexOf(u'/') + 1); }

QFont projectedFont(const Snapshot& snapshot, bool fixed) {
  QFont font(QString::fromStdString(fixed ? snapshot.monospace_font_family : snapshot.ui_font_family));
  font.setPointSize(fixed ? snapshot.monospace_font_point_size : snapshot.ui_font_point_size);
  return font;
}

std::optional<QString> kdeValue(const QString& path, const QString& key) {  // NOLINT
#ifdef HOLONIGHT_HAVE_KCONFIG
  KConfig config(path, KConfig::SimpleConfig);
  KConfigGroup group(&config, kdeGroup(key));
  const QString member = kdeMember(key);
  if (!group.hasKey(member)) {
    return std::nullopt;
  }
  if (kdeFontKey(member)) {
    return group.readEntry(member, QFont()).toString();
  }
  return group.readEntry(member, QString());
#else
  Q_UNUSED(path)
  Q_UNUSED(key)
  return std::nullopt;
#endif
}

bool setKdeValue(const QString& path, const QString& key, const std::optional<QString>& value) {  // NOLINT
#ifdef HOLONIGHT_HAVE_KCONFIG
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  KConfig config(path, KConfig::SimpleConfig);
  KConfigGroup group(&config, kdeGroup(key));
  const QString member = kdeMember(key);
  if (group.isEntryImmutable(member)) {
    return false;
  }
  if (!value) {
    group.deleteEntry(member, KConfigBase::Notify);
  } else if (kdeFontKey(member)) {
    QFont font;
    if (!font.fromString(*value)) {
      return false;
    }
    group.writeEntry(member, font, KConfigBase::Notify);
  } else {
    group.writeEntry(member, *value, KConfigBase::Notify);
  }
  return config.sync();
#else
  Q_UNUSED(path)
  Q_UNUSED(key)
  Q_UNUSED(value)
  return false;
#endif
}

std::map<QString, QString> kdeProjection(const Snapshot& snapshot) {
  return {
      {QStringLiteral("General/ColorScheme"), QString::fromStdString(snapshot.scheme_id)},
      {QStringLiteral("General/font"), projectedFont(snapshot, false).toString()},
      {QStringLiteral("General/fixed"), projectedFont(snapshot, true).toString()},
      {QStringLiteral("Icons/Theme"), QString::fromStdString(snapshot.icon_theme)},
      {QStringLiteral("Mouse/cursorTheme"), QString::fromStdString(snapshot.cursor_theme)},
  };
}

std::optional<QString> schemaKey(const QString& key, GSettingsSchema** schema_out) {
  const qsizetype slash = key.indexOf(u'/');
  if (slash < 1) {
    return std::nullopt;
  }
  GSettingsSchemaSource* source = g_settings_schema_source_get_default();
  if (source == nullptr) {
    return std::nullopt;
  }
  const QByteArray schema_name = key.left(slash).toUtf8();
  GSettingsSchema* schema = g_settings_schema_source_lookup(source, schema_name.constData(), TRUE);
  if (schema == nullptr) {
    return std::nullopt;
  }
  const QByteArray member = key.mid(slash + 1).toUtf8();
  if (g_settings_schema_has_key(schema, member.constData()) == 0) {
    g_settings_schema_unref(schema);
    return std::nullopt;
  }
  *schema_out = schema;
  return QString::fromUtf8(member);
}

std::optional<QString> getGSetting(const QString& target) {
  GSettingsSchema* schema = nullptr;
  const auto key = schemaKey(target, &schema);
  if (!key) {
    return std::nullopt;
  }
  GSettings* settings = g_settings_new_full(schema, nullptr, nullptr);
  GVariant* value = g_settings_get_value(settings, key->toUtf8().constData());
  gchar* text = g_variant_print(value, FALSE);
  QString result = QString::fromUtf8(text);
  g_free(text);
  g_variant_unref(value);
  g_object_unref(settings);
  g_settings_schema_unref(schema);
  if (result.size() >= 2 && result.front() == u'\'' && result.back() == u'\'') {
    result = result.mid(1, result.size() - 2);
  }
  return result;
}

bool setGSetting(const QString& target, const QString& value) {  // NOLINT
  GSettingsSchema* schema = nullptr;
  const auto key = schemaKey(target, &schema);
  if (!key) {
    return false;
  }
  GSettings* settings = g_settings_new_full(schema, nullptr, nullptr);
  const bool written = g_settings_set_string(settings, key->toUtf8().constData(), value.toUtf8().constData()) != 0;
  g_settings_sync();
  g_object_unref(settings);
  g_settings_schema_unref(schema);
  return written;
}

struct CanonicalInput {
  HoloNight::Config::DocumentSnapshot document;
  std::filesystem::path target;
  Holonight::ResolvedAppearance appearance;
};
bool canonicalCurrent(const QString& path, const CanonicalInput& input) {
  const auto target = HoloNight::Config::resolveDocumentTarget(path.toStdString());
  if (!target || *target.value != input.target) {
    return false;
  }
  const auto current = HoloNight::Config::readDocument(input.target);
  return current && current.value->revision == input.document.revision;
}
std::optional<Snapshot> loadSnapshot(const QString& path, QString* diagnostic, CanonicalInput* input = nullptr) {
  const auto target = HoloNight::Config::resolveDocumentTarget(path.toStdString());
  if (!target) {
    *diagnostic = QStringLiteral("canonical appearance path is unavailable");
    return std::nullopt;
  }
  const auto loaded = HoloNight::Config::readAppearanceDocument(*target.value);
  if (!loaded) {
    *diagnostic = QStringLiteral("canonical appearance is invalid");
    return std::nullopt;
  }
  const auto resolved = Holonight::resolveAppearance(loaded.value.value().appearance);  // NOLINT
  if (!resolved) {
    *diagnostic = QStringLiteral("canonical appearance cannot be resolved");
    return std::nullopt;
  }
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  const QByteArray wire = Holonight::Adapters::serializeSemanticAppearance(
      Holonight::resolveSemanticAppearance(resolved.value.value()));  // NOLINT
  const auto parsed = Holonight::Adapters::parseSemanticAppearance(wire.toStdString());
  if (!parsed) {
    *diagnostic = QStringLiteral("semantic appearance v1 validation failed");
    return std::nullopt;
  }
  if (input != nullptr) {
    *input = {.document = loaded.value->snapshot, .target = *target.value, .appearance = *resolved.value};
    if (!canonicalCurrent(path, *input)) {
      *diagnostic = QStringLiteral("canonical appearance changed while loading");
      return std::nullopt;
    }
  }
  return parsed.value;
}

QJsonObject stateEntry(const std::optional<QString>& original, const QString& last, const QString& kind,
                       const QString& target, const QString& key = {}) {  // NOLINT
  QJsonObject entry{{QStringLiteral("last"), last}, {QStringLiteral("kind"), kind}, {QStringLiteral("target"), target}};
  entry[QStringLiteral("original")] = original ? QJsonValue(*original) : QJsonValue(QJsonValue::Null);
  if (!key.isEmpty()) {
    entry[QStringLiteral("key")] = key;
  }
  return entry;
}

std::optional<QString> originalValue(const QJsonObject& old, const std::optional<QString>& current) {
  if (!old.isEmpty() && old.contains(QStringLiteral("original"))) {
    const QJsonValue value = old.value(QStringLiteral("original"));
    return value.isNull() ? std::nullopt : std::optional<QString>(value.toString());
  }
  return current;
}

#include "labwc_adapter.inc"

void restoreUndo(const QList<Undo>& undo) {
  for (const auto& item : std::views::reverse(undo)) {
    if (item.kind == Undo::Kind::File) {
      setFileValue(item.target, item.value);
    } else if (item.kind == Undo::Kind::LabwcFont) {
      setLabwcFontValue(item.target, item.key, item.value);
    } else if (item.kind == Undo::Kind::Gtk) {
      setIniValue(item.target, item.key, item.value);
    } else if (item.kind == Undo::Kind::Kde) {
      setKdeValue(item.target, item.key, item.value);
    } else if (item.value) {
      setGSetting(item.target, *item.value);  // NOLINT(bugprone-unchecked-optional-access)
    }
  }
}

bool applyGSettings(const Tier1Projection& projection, QJsonObject& entries, QList<Output>& outputs,
                    QList<Undo>& undo) {
  for (const auto& [target_value, projected] : projection.gsettings) {
    const QString target = QString::fromStdString(target_value);
    const QString desired = QString::fromStdString(projected);
    const auto current = getGSetting(target);
    if (!current) {
      outputs.append({
          .name = target,
          .status = QStringLiteral("unavailable"),
          .mode = QStringLiteral("live"),
          .diagnostic = QStringLiteral("schema or key is unavailable"),
      });
      continue;
    }
    if (*current != desired && !setGSetting(target, desired)) {
      restoreUndo(undo);
      outputs.append({
          .name = target,
          .status = QStringLiteral("error"),
          .mode = QStringLiteral("live"),
          .diagnostic = QStringLiteral("writable GSettings output rejected the update"),
      });
      return false;
    }
    if (*current != desired) {
      undo.append({.kind = Undo::Kind::GSettings, .target = target, .key = {}, .value = current});
    }
    const QJsonObject old = entries.value(target).toObject();
    entries[target] = stateEntry(originalValue(old, current), desired, QStringLiteral("gsettings"), target);
    outputs.append({
        .name = target,
        .status = *current == desired ? QStringLiteral("unchanged") : QStringLiteral("applied"),
        .mode = QStringLiteral("live"),
        .diagnostic = {},
    });
  }

  return true;
}

bool applyGtkSettings(const Tier1Projection& projection, QJsonObject& entries, QList<Output>& outputs,
                      QList<Undo>& undo) {
  const QList<std::pair<QString, const std::map<std::string, std::string>*>> gtk{
      {configHome() + QStringLiteral("/gtk-3.0/settings.ini"), &projection.gtk3_settings},
      {configHome() + QStringLiteral("/gtk-4.0/settings.ini"), &projection.gtk4_settings},
  };
  for (const auto& [path, values] : gtk) {
    const QString major = path.contains(QStringLiteral("gtk-3.0")) ? QStringLiteral("gtk3") : QStringLiteral("gtk4");
    for (const auto& [key_value, projected] : *values) {
      const QString key = QString::fromStdString(key_value);
      const QString desired = QString::fromStdString(projected);
      const auto current = iniValue(path, key);
      if (!setIniValue(path, key, desired)) {
        restoreUndo(undo);
        outputs.append({
            .name = major + u'/' + key,
            .status = QStringLiteral("error"),
            .mode = QStringLiteral("relaunch"),
            .diagnostic = QStringLiteral("GTK settings file could not be updated atomically"),
        });
        return false;
      }
      if (current != std::optional<QString>(desired)) {
        undo.append({.kind = Undo::Kind::Gtk, .target = path, .key = key, .value = current});
      }
      const QString output_id = major + u'/' + key;
      const QJsonObject old = entries.value(output_id).toObject();
      entries[output_id] = stateEntry(originalValue(old, current), desired, QStringLiteral("gtk"), path, key);
      outputs.append({
          .name = output_id,
          .status =
              current == std::optional<QString>(desired) ? QStringLiteral("unchanged") : QStringLiteral("applied"),
          .mode = QStringLiteral("relaunch"),
          .diagnostic = {},
      });
    }
  }

  return true;
}

bool applyKdeSettings(const Snapshot& snapshot, QJsonObject& entries, QList<Output>& outputs, QList<Undo>& undo) {
#ifdef HOLONIGHT_HAVE_KCONFIG
  const QString kde_path = configHome() + QStringLiteral("/kdeglobals");
  for (const auto& [key, desired] : kdeProjection(snapshot)) {
    const QString output_id = QStringLiteral("kde/") + key;
    const auto current = kdeValue(kde_path, key);
    if (current != std::optional<QString>(desired) && !setKdeValue(kde_path, key, desired)) {
      restoreUndo(undo);
      outputs.append({
          .name = output_id,
          .status = QStringLiteral("error"),
          .mode = QStringLiteral("relaunch"),
          .diagnostic = QStringLiteral("KDE configuration could not be updated"),
      });
      return false;
    }
    if (current != std::optional<QString>(desired)) {
      undo.append({.kind = Undo::Kind::Kde, .target = kde_path, .key = key, .value = current});
    }
    const QJsonObject old = entries.value(output_id).toObject();
    entries[output_id] = stateEntry(originalValue(old, current), desired, QStringLiteral("kde"), kde_path, key);
    outputs.append({
        .name = output_id,
        .status = current == std::optional<QString>(desired) ? QStringLiteral("unchanged") : QStringLiteral("applied"),
        .mode = QStringLiteral("relaunch"),
        .diagnostic = {},
    });
  }
#else
  outputs.append({
      .name = QStringLiteral("kde"),
      .status = QStringLiteral("unavailable"),
      .mode = QStringLiteral("relaunch"),
      .diagnostic = QStringLiteral("KDE ConfigCore dependency was unavailable at build time"),
  });
#endif
  return true;
}

int sourceChanged(QList<Output>& outputs, QList<Undo>& undo, const std::optional<QJsonObject>& previous_state,
                  bool state_existed) {
  restoreUndo(undo);
  finishLabwcRecovery();
  if (previous_state && !(state_existed ? writeJsonObject(statePath(), *previous_state) : QFile::remove(statePath()))) {
    outputs.append({
        .name = "state",
        .status = "error",
        .mode = "live",
        .diagnostic = "adapter state could not be restored after an external appearance edit",
    });
  }
  outputs.append({
      .name = "canonical",
      .status = "error",
      .mode = "live",
      .diagnostic = "appearance changed during application; the external document was preserved",
  });
  return respond(QStringLiteral("apply"), outputs, true);
}

int apply(const QString& appearance) {
  QString diagnostic;
  CanonicalInput input;
  const auto snapshot = loadSnapshot(appearance, &diagnostic, &input);
  if (!snapshot) {
    return respond(QStringLiteral("apply"),
                   {
                       {
                           .name = QStringLiteral("canonical"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = diagnostic,
                       },
                   },
                   true);
  }
  const Tier1Projection projection = Holonight::Adapters::projectTier1(*snapshot);
  const auto state_document = readJsonObject(statePath());
  if (!state_document) {
    return respond(QStringLiteral("apply"),
                   {
                       {
                           .name = QStringLiteral("state"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = QStringLiteral("state is corrupt or unreadable"),
                       },
                   },
                   true);
  }
  const bool state_existed = QFileInfo::exists(statePath());
  QJsonObject state = *state_document;
  QJsonObject entries = state.value(QStringLiteral("entries")).toObject();
  QList<Output> outputs;
  QList<Undo> undo;

  if (!applyGSettings(projection, entries, outputs, undo)) {
    return respond(QStringLiteral("apply"), outputs, true);
  }

  if (!applyGtkSettings(projection, entries, outputs, undo)) {
    return respond(QStringLiteral("apply"), outputs, true);
  }

  if (!applyKdeSettings(*snapshot, entries, outputs, undo)) {
    return respond(QStringLiteral("apply"), outputs, true);
  }
  if (!applyLabwc(input.appearance, entries, outputs, undo)) {
    restoreUndo(undo);
    finishLabwcRecovery();
    outputs.append({
        .name = "labwc/theme",
        .status = "error",
        .mode = "live",
        .diagnostic = "labwc update failed; transaction rolled back",
    });
    return respond("apply", outputs, true);
  }
  if (!canonicalCurrent(appearance, input)) {
    return sourceChanged(outputs, undo, std::nullopt, state_existed);
  }
  state[QStringLiteral("protocol_version")] = kProtocolVersion;
  state[QStringLiteral("entries")] = entries;
  if (!writeJsonObject(statePath(), state)) {
    restoreUndo(undo);
    finishLabwcRecovery();
    outputs.append({
        .name = QStringLiteral("state"),
        .status = QStringLiteral("error"),
        .mode = QStringLiteral("live"),
        .diagnostic = QStringLiteral("adapter state could not be stored atomically"),
    });
    return respond(QStringLiteral("apply"), outputs, true);
  }
  if (!canonicalCurrent(appearance, input)) {
    return sourceChanged(outputs, undo, state_document, state_existed);
  }
  finishLabwcRecovery();
  if (std::ranges::any_of(
          undo, [](const Undo& item) { return item.kind == Undo::Kind::File || item.kind == Undo::Kind::LabwcFont; })) {
    reloadLabwc(outputs);
  }
  if (!canonicalCurrent(appearance, input)) {
    return sourceChanged(outputs, undo, state_document, state_existed);
  }
  outputs.append({
      .name = QStringLiteral("portal"),
      .status = QStringLiteral("delegated"),
      .mode = QStringLiteral("delegated"),
      .diagnostic = QStringLiteral("published by the HoloNight Shell Settings portal"),
  });
  outputs.append({
      .name = QStringLiteral("xsettings"),
      .status = QStringLiteral("unavailable"),
      .mode = QStringLiteral("delegated"),
      .diagnostic = QStringLiteral("no competing XSettings manager is started"),
  });
  outputs.append({
      .name = QStringLiteral("application-styling"),
      .status = QStringLiteral("application-owned"),
      .mode = QStringLiteral("relaunch"),
      .diagnostic = QStringLiteral("native and libadwaita styling is preserved"),
  });
  outputs.append({
      .name = QStringLiteral("XCURSOR_THEME"),
      .status = QStringLiteral("delegated"),
      .mode = QStringLiteral("session-restart"),
      .diagnostic = QStringLiteral("exported by session startup integration"),
  });
  return respond(QStringLiteral("apply"), outputs);
}

std::optional<QString> entryCurrent(const QJsonObject& entry) {
  const QString kind = entry.value(QStringLiteral("kind")).toString();
  const QString target = entry.value(QStringLiteral("target")).toString();
  const QString key = entry.value(QStringLiteral("key")).toString();
  if (kind.startsWith("labwc-")) {
    return labwcCurrent(entry);
  }
  if (kind == QStringLiteral("gtk")) {
    return iniValue(target, key);
  }
  if (kind == QStringLiteral("kde")) {
    return kdeValue(target, key);
  }
  return getGSetting(target);
}

bool setEntryValue(const QJsonObject& entry, const std::optional<QString>& value) {
  const QString kind = entry.value(QStringLiteral("kind")).toString();
  const QString target = entry.value(QStringLiteral("target")).toString();
  const QString key = entry.value(QStringLiteral("key")).toString();
  if (kind.startsWith("labwc-")) {
    return labwcSet(entry, value);
  }
  if (kind == QStringLiteral("gtk")) {
    return setIniValue(target, key, value);
  }
  if (kind == QStringLiteral("kde")) {
    return setKdeValue(target, key, value);
  }
  return value && setGSetting(target, *value);
}

Undo::Kind undoKind(const QString& kind) {
  if (kind == "labwc-file") {
    return Undo::Kind::File;
  }
  if (kind == "labwc-font") {
    return Undo::Kind::LabwcFont;
  }
  if (kind == "gtk") {
    return Undo::Kind::Gtk;
  }
  if (kind == "kde") {
    return Undo::Kind::Kde;
  }
  return Undo::Kind::GSettings;
}

QString statusDiagnostic(bool disabled, bool redirected, bool stale, bool available) {
  if (disabled) {
    return QStringLiteral("HoloNight is no longer selected");
  }
  if (redirected) {
    return QStringLiteral("configuration or data paths changed since last apply");
  }
  if (stale) {
    return QStringLiteral("canonical appearance changed since last apply");
  }
  if (available) {
    return {};
  }
  return QStringLiteral("output is absent or unavailable");
}

std::map<QString, QString> expectedProjection(const Snapshot& snapshot) {
  std::map<QString, QString> expected;
  const Tier1Projection projection = Holonight::Adapters::projectTier1(snapshot);
  for (const auto& [key, value] : projection.gsettings) {
    expected[QString::fromStdString(key)] = QString::fromStdString(value);
  }
  for (const auto& [key, value] : projection.gtk3_settings) {
    expected[QStringLiteral("gtk3/") + QString::fromStdString(key)] = QString::fromStdString(value);
  }
  for (const auto& [key, value] : projection.gtk4_settings) {
    expected[QStringLiteral("gtk4/") + QString::fromStdString(key)] = QString::fromStdString(value);
  }
  for (const auto& [key, value] : kdeProjection(snapshot)) {
    expected[QStringLiteral("kde/") + key] = value;
  }
  return expected;
}

QMap<QString, QByteArray> labwcStatusFiles(const QJsonObject& state, const std::optional<QString>& appearance,
                                           std::map<QString, QString>& expected) {
  QMap<QString, QByteArray> labwcFiles;
  const auto labwcValues = appearance ? labwcExpected(*appearance, &labwcFiles) : QMap<QString, QString>{};
  for (auto it = labwcValues.begin(); it != labwcValues.end(); ++it) {
    expected[it.key()] = it.value();
  }
  if (labwcFiles.isEmpty()) {
    const auto themeEntry = state.value("entries").toObject().value("labwc/file/themerc").toObject();
    if (!themeEntry.isEmpty()) {
      labwcFiles["themerc"] = themeEntry["last"].toString().toUtf8();
    }
  }
  return labwcFiles;
}

int status(const std::optional<QString>& appearance) {
  std::optional<Snapshot> snapshot;
  if (appearance) {
    QString diagnostic;
    snapshot = loadSnapshot(*appearance, &diagnostic);
    if (!snapshot) {
      return respond(QStringLiteral("status"),
                     {
                         {
                             .name = QStringLiteral("canonical"),
                             .status = QStringLiteral("error"),
                             .mode = QStringLiteral("live"),
                             .diagnostic = diagnostic,
                         },
                     },
                     true);
    }
  }
  auto expected = snapshot ? expectedProjection(*snapshot) : std::map<QString, QString>{};
  const auto state = readJsonObject(statePath());
  if (!state) {
    return respond(QStringLiteral("status"),
                   {
                       {
                           .name = QStringLiteral("state"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = QStringLiteral("state is corrupt or unreadable"),
                       },
                   },
                   true);
  }
  QList<Output> outputs;
  const bool selected = labwcSelected(outputs);
  auto labwcFiles = labwcStatusFiles(*state, appearance, expected);
  if (!labwcFiles.isEmpty()) {
    labwcOverrides(labwcFiles, outputs);
  }
  if (selected && !state->value("entries").toObject().contains("labwc/file/themerc")) {
    outputs.append({
        .name = "labwc/theme",
        .status = "unavailable",
        .mode = "live",
        .diagnostic = "synchronized theme has not been applied; installed fallback may be in use",
    });
  }
  const QJsonObject entries = state->value(QStringLiteral("entries")).toObject();
  for (auto iterator = entries.begin(); iterator != entries.end(); ++iterator) {
    const QJsonObject entry = iterator.value().toObject();
    const QString kind = entry.value(QStringLiteral("kind")).toString();
    const auto current = entryCurrent(entry);
    const QString mode = kind == QStringLiteral("gtk") || kind == QStringLiteral("kde") ? QStringLiteral("relaunch")
                                                                                        : QStringLiteral("live");
    const bool disabled = kind.startsWith("labwc-") && !selected;
    const bool redirected =
        (kind == "labwc-file" && entry["target"].toString() != labwcThemeDirectory() + '/' + iterator.key().mid(11)) ||
        (kind == "labwc-font" && entry["target"].toString() != labwcDirectory() + "/rc.xml");
    const bool stale = disabled || redirected ||
                       (snapshot && expected.contains(iterator.key()) &&
                        expected.at(iterator.key()) != entry.value(QStringLiteral("last")).toString());
    outputs.append({
        .name = iterator.key(),
        .status = !stale && current == std::optional<QString>(entry.value(QStringLiteral("last")).toString())
                      ? QStringLiteral("applied")
                      : QStringLiteral("conflict"),
        .mode = mode,
        .diagnostic = statusDiagnostic(disabled, redirected, stale, current.has_value()),
    });
  }
  if (outputs.isEmpty()) {
    outputs.append({
        .name = QStringLiteral("state"),
        .status = QStringLiteral("unavailable"),
        .mode = QStringLiteral("live"),
        .diagnostic = QStringLiteral("appearance has not been applied"),
    });
  }
#ifndef HOLONIGHT_HAVE_KCONFIG
  outputs.append({
      .name = QStringLiteral("kde"),
      .status = QStringLiteral("unavailable"),
      .mode = QStringLiteral("relaunch"),
      .diagnostic = QStringLiteral("KDE ConfigCore dependency was unavailable at build time"),
  });
#endif
  return respond(QStringLiteral("status"), outputs);
}

int revert() {
  const auto state = readJsonObject(statePath());
  if (!state) {
    return respond(QStringLiteral("revert"),
                   {
                       {
                           .name = QStringLiteral("state"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = QStringLiteral("state is corrupt or unreadable"),
                       },
                   },
                   true);
  }
  QJsonObject remaining;
  QList<Output> outputs;
  QList<Undo> undo;
  const QJsonObject entries = state->value(QStringLiteral("entries")).toObject();
  for (auto iterator = entries.begin(); iterator != entries.end(); ++iterator) {
    const QJsonObject entry = iterator.value().toObject();
    const QString kind = entry.value(QStringLiteral("kind")).toString();
    const QString target = entry.value(QStringLiteral("target")).toString();
    const QString key = entry.value(QStringLiteral("key")).toString();
    const QString last = entry.value(QStringLiteral("last")).toString();
    const auto current = entryCurrent(entry);
    const QString mode = kind == QStringLiteral("gtk") || kind == QStringLiteral("kde") ? QStringLiteral("relaunch")
                                                                                        : QStringLiteral("live");
    if (current != std::optional<QString>(last)) {
      remaining[iterator.key()] = entry;
      outputs.append({
          .name = iterator.key(),
          .status = QStringLiteral("conflict"),
          .mode = mode,
          .diagnostic = QStringLiteral("externally modified value was preserved"),
      });
      continue;
    }
    const QJsonValue original_json = entry.value(QStringLiteral("original"));
    const std::optional<QString> original =
        original_json.isNull() ? std::nullopt : std::optional<QString>(original_json.toString());
    const bool written = setEntryValue(entry, original);
    if (!written) {
      remaining[iterator.key()] = entry;
      outputs.append({
          .name = iterator.key(),
          .status = QStringLiteral("error"),
          .mode = mode,
          .diagnostic = QStringLiteral("owned output could not be restored"),
      });
      continue;
    }
    undo.append({
        .kind = undoKind(kind),
        .target = target,
        .key = key,
        .value = current,
    });
    outputs.append({.name = iterator.key(), .status = QStringLiteral("restored"), .mode = mode, .diagnostic = {}});
  }
  QJsonObject updated{{QStringLiteral("protocol_version"), kProtocolVersion}, {QStringLiteral("entries"), remaining}};
  if (!writeJsonObject(statePath(), updated)) {
    restoreUndo(undo);
    return respond(QStringLiteral("revert"), outputs, true);
  }
  if (std::ranges::any_of(
          undo, [](const Undo& item) { return item.kind == Undo::Kind::File || item.kind == Undo::Kind::LabwcFont; })) {
    reloadLabwc(outputs);
  }
  const bool error =
      std::ranges::any_of(outputs, [](const Output& output) { return output.status == QStringLiteral("error"); });
  return respond(QStringLiteral("revert"), outputs, error);
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  QCoreApplication application(argc, argv);
#ifdef HOLONIGHT_HAVE_KCONFIG
  // ConfigGui registers the QFont codecs used by KConfigGroup at library load time.
  Q_UNUSED(KConfigGui::hasSessionConfig())
#endif
  const QStringList arguments = QCoreApplication::arguments();
  if (arguments.size() < 2) {
    return respond(QStringLiteral("unknown"),
                   {
                       {
                           .name = QStringLiteral("cli"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = QStringLiteral("an operation is required"),
                       },
                   },
                   true);
  }
  const QString& operation = arguments[1];
  const auto labwc_index = arguments.indexOf("--labwc-config");
  if (labwc_index >= 0) {
    if (labwc_index + 1 >= arguments.size()) {
      return respond(
          operation,
          {{.name = "cli", .status = "error", .mode = "live", .diagnostic = "--labwc-config requires a directory"}},
          true);
    }
    labwcConfig() = QDir(arguments[labwc_index + 1]).absolutePath();
  }
  QDir().mkpath(QFileInfo(statePath()).absolutePath());
  QLockFile lock(statePath() + ".lock");
  if (operation != "query" && !lock.tryLock(10000)) {
    return respond(
        operation,
        {{.name = "state", .status = "error", .mode = "live", .diagnostic = "adapter state lock is unavailable"}},
        true);
  }
  if (operation != "query" && !recoverLabwc()) {
    return respond(
        operation,
        {
            {
                .name = "labwc/recovery",
                .status = "error",
                .mode = "live",
                .diagnostic = "interrupted labwc update could not be recovered; external changes were preserved",
            },
        },
        true);
  }
  if (operation == QStringLiteral("status")) {
    const qsizetype index = arguments.indexOf(QStringLiteral("--appearance"));
    if (index >= 0 && index + 1 >= arguments.size()) {
      return respond(operation,
                     {
                         {
                             .name = QStringLiteral("cli"),
                             .status = QStringLiteral("error"),
                             .mode = QStringLiteral("live"),
                             .diagnostic = QStringLiteral("--appearance requires a path"),
                         },
                     },
                     true);
    }
    return status(index < 0 ? std::nullopt : std::optional<QString>(arguments[index + 1]));
  }
  if (operation == QStringLiteral("revert")) {
    return revert();
  }
  const qsizetype appearance_index = arguments.indexOf(QStringLiteral("--appearance"));
  if (appearance_index < 0 || appearance_index + 1 >= arguments.size()) {
    return respond(operation,
                   {
                       {
                           .name = QStringLiteral("cli"),
                           .status = QStringLiteral("error"),
                           .mode = QStringLiteral("live"),
                           .diagnostic = QStringLiteral("--appearance requires a path"),
                       },
                   },
                   true);
  }
  const QString& appearance = arguments[appearance_index + 1];
  if (operation == QStringLiteral("apply")) {
    return apply(appearance);
  }
  if (operation == QStringLiteral("query")) {
    const qsizetype field_index = arguments.indexOf(QStringLiteral("--field"));
    QString diagnostic;
    const auto snapshot = loadSnapshot(appearance, &diagnostic);
    if (field_index < 0 || field_index + 1 >= arguments.size() ||
        arguments[field_index + 1] != QStringLiteral("cursor-theme") || !snapshot) {
      return 1;
    }
    QFile standard_output;
    if (!standard_output.open(stdout, QIODevice::WriteOnly)) {
      return 1;
    }
    standard_output.write(QByteArray::fromStdString(snapshot->cursor_theme));
    standard_output.write("\n");
    return 0;
  }
  return respond(operation,
                 {
                     {
                         .name = QStringLiteral("cli"),
                         .status = QStringLiteral("error"),
                         .mode = QStringLiteral("live"),
                         .diagnostic = QStringLiteral("unknown operation"),
                     },
                 },
                 true);
}
