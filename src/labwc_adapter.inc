// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
// Included inside adapter_main's private namespace to share transaction and protocol helpers.
QString labwcConfig;
QString labwcDirectory() {
  if (!labwcConfig.isEmpty())
    return labwcConfig;
  QStringList dirs{configHome()};
  dirs += qEnvironmentVariable("XDG_CONFIG_DIRS", "/etc/xdg").split(':', Qt::SkipEmptyParts);
  for (const auto &dir : dirs)
    if (QFileInfo::exists(dir + "/labwc/rc.xml"))
      return dir + "/labwc";
  return configHome() + "/labwc";
}
QString labwcThemeDirectory() {
  return qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share") + "/themes/HoloNight/labwc";
}
std::optional<QString> fileValue(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;
  return QString::fromUtf8(file.readAll());
}
bool setFileValue(const QString &path, const std::optional<QString> &value) {
  if (!value)
    return !QFileInfo::exists(path) || QFile::remove(path);
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    return false;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  const auto bytes = value->toUtf8();
  return file.write(bytes) == bytes.size() && file.commit();
}
bool labwcDocument(const QString &path, QDomDocument &doc) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return false;
  return bool(doc.setContent(&file)) &&
         (doc.documentElement().tagName() == "labwc_config" || doc.documentElement().tagName() == "openbox_config");
}
QDomElement fontElement(QDomDocument &doc, const QString &place) {
  const auto theme = doc.documentElement().firstChildElement("theme");
  for (auto font = theme.firstChildElement("font"); !font.isNull(); font = font.nextSiblingElement("font"))
    if (font.attribute("place") == place)
      return font;
  return {};
}
// File path followed by the XML key, matching the other adapter accessors.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::optional<QString> labwcFontValue(const QString &path, const QString &key) {
  QDomDocument doc;
  if (!labwcDocument(path, doc))
    return std::nullopt;
  const auto element = fontElement(doc, key.section('/', 0, 0)).firstChildElement(key.section('/', 1, 1));
  return element.isNull() ? std::nullopt : std::optional<QString>(element.text());
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool setLabwcFontValue(const QString &path, const QString &key, const std::optional<QString> &value) {
  QDomDocument doc;
  if (!labwcDocument(path, doc))
    return false;
  auto theme = doc.documentElement().firstChildElement("theme");
  auto font = fontElement(doc, key.section('/', 0, 0));
  if (font.isNull()) {
    if (!value)
      return true;
    font = doc.createElement("font");
    font.setAttribute("place", key.section('/', 0, 0));
    theme.appendChild(font);
  }
  auto element = font.firstChildElement(key.section('/', 1, 1));
  if (!element.isNull())
    font.removeChild(element);
  if (value) {
    element = doc.createElement(key.section('/', 1, 1));
    element.appendChild(doc.createTextNode(*value));
    font.appendChild(element);
  }
  if (!font.hasChildNodes())
    theme.removeChild(font);
  return setFileValue(path, doc.toString(2));
}
std::optional<QString> labwcCurrent(const QJsonObject &entry) {
  return entry["kind"].toString() == "labwc-file" ? fileValue(entry["target"].toString())
                                                  : labwcFontValue(entry["target"].toString(), entry["key"].toString());
}
bool labwcSet(const QJsonObject &entry, const std::optional<QString> &value) {
  return entry["kind"].toString() == "labwc-file"
             ? setFileValue(entry["target"].toString(), value)
             : setLabwcFontValue(entry["target"].toString(), entry["key"].toString(), value);
}
bool labwcSelected(QList<Output> &outputs) {
  const QString path = labwcDirectory() + "/rc.xml";
  QDomDocument doc;
  if (!QFileInfo::exists(path)) {
    outputs.append(
        {"labwc/selection", "delegated", "delegated", "select HoloNight in rc.xml to enable synchronization"});
    return false;
  }
  if (!labwcDocument(path, doc)) {
    outputs.append({"labwc/selection", "conflict", "live", "rc.xml is malformed or unreadable; preserved"});
    return false;
  }
  const auto name = doc.documentElement().firstChildElement("theme").firstChildElement("name").text().trimmed();
  outputs.append({"labwc/selection", name == "HoloNight" ? "applied" : "delegated", "delegated",
                  name == "HoloNight" ? QString{} : "HoloNight is not selected; synchronization is disabled"});
  return name == "HoloNight";
}
void labwcOverrides(const QMap<QString, QByteArray> &files, QList<Output> &outputs) {
  const auto contents = fileValue(labwcDirectory() + "/themerc-override");
  if (!contents)
    return;
  QStringList overlap;
  const auto theme = QString::fromUtf8(files["themerc"]);
  for (const auto &raw : contents->split('\n')) {
    const auto key = raw.section(':', 0, 0).trimmed();
    if (!raw.contains(':') || key.startsWith('#') || key.startsWith('!'))
      continue;
    const auto pattern = QRegularExpression(QRegularExpression::wildcardToRegularExpression(key));
    for (const auto &line : theme.split('\n'))
      if (pattern.match(line.section(':', 0, 0)).hasMatch()) {
        overlap.append(key);
        break;
      }
    if (key == "border.color")
      overlap.append(key);
  }
  if (!overlap.isEmpty())
    outputs.append(
        {"labwc/overrides", "conflict", "live", "preserved themerc-override overlaps: " + overlap.join(", ")});
}
QMap<QString, QString> labwcExpected(const QString &appearance, QMap<QString, QByteArray> *filesOut = nullptr) {
  const auto loaded = HoloNight::Config::load(appearance.toStdString());
  if (!loaded || !loaded.value.has_value())
    return {};
  const auto resolved = Holonight::resolveAppearance(loaded.value->appearance);
  if (!resolved || !resolved.value.has_value())
    return {};
  auto files = Holonight::Adapters::labwcTheme(Holonight::resolveSemanticAppearance(*resolved.value));
  QMap<QString, QString> expected;
  for (auto it = files.begin(); it != files.end(); ++it)
    expected["labwc/file/" + it.key()] = QString::fromUtf8(it.value());
  for (const QString &place : {QStringLiteral("ActiveWindow"), QStringLiteral("InactiveWindow")}) {
    expected["labwc/font/" + place + "/name"] = resolved.value->title_font;
    expected["labwc/font/" + place + "/size"] = QString::number(qRound(resolved.value->title_font_size * 96.0 / 72.0));
  }
  if (filesOut)
    *filesOut = files;
  return expected;
}
QString labwcRecoveryPath() { return statePath() + ".labwc-recovery.json"; }
void finishLabwcRecovery() {
  const auto journal = readJsonObject(labwcRecoveryPath());
  if (journal && !journal->isEmpty()) {
    const QString backup = journal->value("backup").toString();
    const QString stage = journal->value("stage").toString();
    if (!backup.isEmpty())
      QDir(backup).removeRecursively();
    if (!stage.isEmpty())
      QDir(stage).removeRecursively();
  }
  QFile::remove(labwcRecoveryPath());
}
bool recoverLabwc() {
  const auto journal = readJsonObject(labwcRecoveryPath());
  if (!journal)
    return false;
  if (journal->isEmpty())
    return true;
  const auto state = readJsonObject(statePath());
  if (!state)
    return false;
  const auto intended = journal->value("entries").toObject();
  const auto committedEntries = state->value("entries").toObject();
  bool committed = true;
  for (auto it = intended.begin(); it != intended.end(); ++it)
    if (committedEntries[it.key()] != it.value())
      committed = false;
  if (!committed) {
    const QString directory = journal->value("directory").toString();
    const QString backup = journal->value("backup").toString();
    if (!QDir(directory).exists() && QDir(backup).exists() && !QDir().rename(backup, directory))
      return false;
    const auto before = journal->value("before").toObject();
    for (auto it = before.begin(); it != before.end(); ++it) {
      const auto entry = it.value().toObject();
      const auto current = labwcCurrent(entry);
      const auto original = originalValue(entry, std::nullopt);
      if (current == original)
        continue;
      if (current != std::optional<QString>(entry["last"].toString()) || !labwcSet(entry, original))
        return false;
    }
  }
  finishLabwcRecovery();
  return true;
}
bool applyLabwc(const QString &appearance, QJsonObject &entries, QList<Output> &outputs, QList<Undo> &undo) {
  if (!labwcSelected(outputs))
    return true;
  QMap<QString, QByteArray> files;
  const auto expected = labwcExpected(appearance, &files);
  labwcOverrides(files, outputs);
  const QString directory = labwcThemeDirectory();
  const QString rc = labwcDirectory() + "/rc.xml";
  // A user-prefix installation can occupy the synchronized path. Recognize only
  // the complete, unchanged default fallback and retain its bytes for revert.
  bool installedFallback = false;
  const auto defaultAppearance = Holonight::resolveAppearance(HoloNight::Config::Appearance{});
  if (defaultAppearance.value.has_value()) {
    const auto fallback =
        Holonight::Adapters::labwcTheme(Holonight::resolveSemanticAppearance(*defaultAppearance.value));
    installedFallback =
        QDir(directory).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden).size() ==
        fallback.size();
    for (auto it = fallback.begin(); it != fallback.end(); ++it)
      if (fileValue(directory + '/' + it.key()) != std::optional<QString>(QString::fromUtf8(it.value())))
        installedFallback = false;
  }
  // Refuse the whole labwc transaction on any ownership conflict.
  for (auto it = expected.begin(); it != expected.end(); ++it) {
    const bool isFile = it.key().startsWith("labwc/file/");
    const auto old = entries[it.key()].toObject();
    const auto current = isFile ? fileValue(directory + '/' + it.key().mid(11)) : labwcFontValue(rc, it.key().mid(11));
    if ((!old.isEmpty() && (old["target"].toString() != (isFile ? directory + '/' + it.key().mid(11) : rc) ||
                            current != std::optional<QString>(old["last"].toString()))) ||
        (isFile && old.isEmpty() && !installedFallback && QFileInfo::exists(directory + '/' + it.key().mid(11)))) {
      outputs.append({it.key(), "conflict", "live", "externally modified or unowned output was preserved"});
      return true;
    }
  }
  // Stage every asset, then replace the directory using same-filesystem renames.
  if (!QDir().mkpath(QFileInfo(directory).absolutePath()))
    return false;
  QTemporaryDir stage(QFileInfo(directory).absolutePath() + "/.holonight-stage-XXXXXX");
  if (!stage.isValid())
    return false;
  if (QFileInfo(directory).isSymLink())
    return false;
  const auto existing = QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
  for (const auto &file : existing) {
    if (!file.isFile() || file.isSymLink() || !QFile::copy(file.filePath(), stage.path() + '/' + file.fileName()))
      return false;
  }
  for (auto it = files.begin(); it != files.end(); ++it)
    if (!setFileValue(stage.path() + '/' + it.key(), QString::fromUtf8(it.value())))
      return false;
  bool changed = false;
  QJsonObject before;
  QJsonObject intended;
  for (auto it = expected.begin(); it != expected.end(); ++it) {
    bool isFile = it.key().startsWith("labwc/file/");
    QString key = it.key().mid(11);
    QString target = isFile ? directory + '/' + key : rc;
    const auto current = isFile ? fileValue(target) : labwcFontValue(target, key);
    const auto old = entries[it.key()].toObject();
    entries[it.key()] = stateEntry(originalValue(old, current), it.value(), isFile ? "labwc-file" : "labwc-font",
                                   target, isFile ? QString{} : key);
    intended[it.key()] = entries[it.key()];
    before[it.key()] =
        stateEntry(current, it.value(), isFile ? "labwc-file" : "labwc-font", target, isFile ? QString{} : key);
    if (current != std::optional<QString>(it.value())) {
      undo.append({isFile ? Undo::Kind::File : Undo::Kind::LabwcFont, target, isFile ? QString{} : key, current});
      changed = true;
    }
  }
  if (changed) {
    const QString backup = stage.path() + "-previous";
    if (!writeJsonObject(labwcRecoveryPath(), {{"entries", intended},
                                               {"before", before},
                                               {"directory", directory},
                                               {"backup", backup},
                                               {"stage", stage.path()}}))
      return false;
    const bool exists = QDir(directory).exists();
    if (exists && !QDir().rename(directory, backup))
      return false;
    if (!QDir().rename(stage.path(), directory)) {
      if (exists)
        QDir().rename(backup, directory);
      return false;
    }
    stage.setAutoRemove(false);

    for (auto it = expected.begin(); it != expected.end(); ++it)
      if (it.key().startsWith("labwc/font/") && !setLabwcFontValue(rc, it.key().mid(11), it.value()))
        return false;
  }
  outputs.append({"labwc/theme", changed ? "applied" : "unchanged", "live", {}});
  return true;
}
void reloadLabwc(QList<Output> &outputs) {
  // LABWC_PID is supplied by labwc to its session children. Validate ownership and executable.
  bool valid = false;
  const auto pid = qEnvironmentVariable("LABWC_PID").toLongLong(&valid);
  if (!valid || pid <= 1)
    return;
  const QString proc = "/proc/" + QString::number(pid);
  if (QFileInfo(proc).ownerId() != uint(getuid()) ||
      QFileInfo(QFile::symLinkTarget(proc + "/exe")).fileName() != "labwc")
    return;
  if (::kill(pid_t(pid), SIGHUP) != 0)
    outputs.append({"labwc/reload", "unavailable", "live", "reload failed; generated assets remain valid"});
}
