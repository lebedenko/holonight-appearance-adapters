// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
// Included inside adapter_main's private namespace to share transaction and protocol helpers.
QString& labwcConfig() {
  static QString directory;
  return directory;
}
QString labwcDirectory() {
  if (!labwcConfig().isEmpty()) {
    return labwcConfig();
  }
  const auto managed = qEnvironmentVariable("HOLONIGHT_LABWC_CONFIG_DIR");
  if (!managed.isEmpty()) {
    return managed;
  }
  QStringList dirs{configHome()};
  dirs += qEnvironmentVariable("XDG_CONFIG_DIRS", "/etc/xdg").split(':', Qt::SkipEmptyParts);
  for (const auto& dir : dirs) {
    if (QFileInfo::exists(dir + "/labwc/rc.xml")) {
      return dir + "/labwc";
    }
  }
  return configHome() + "/labwc";
}
QString labwcThemeDirectory() {
  return qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share") + "/themes/HoloNight/labwc";
}
std::optional<QString> fileValue(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  return QString::fromUtf8(file.readAll());
}
bool setFileValue(const QString& path, const std::optional<QString>& value) {
  if (!value) {
    return !QFileInfo::exists(path) || QFile::remove(path);
  }
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  const auto bytes = value->toUtf8();
  return file.write(bytes) == bytes.size() && file.commit();
}
bool labwcDocument(const QString& path, QDomDocument& doc) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  return static_cast<bool>(doc.setContent(&file)) &&
         (doc.documentElement().tagName() == "labwc_config" || doc.documentElement().tagName() == "openbox_config");
}
QDomElement fontElement(QDomDocument& doc, const QString& place) {
  const auto theme = doc.documentElement().firstChildElement("theme");
  for (auto font = theme.firstChildElement("font"); !font.isNull(); font = font.nextSiblingElement("font")) {
    if (font.attribute("place") == place) {
      return font;
    }
  }
  return {};
}
// File path followed by the XML key, matching the other adapter accessors.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::optional<QString> labwcFontValue(const QString& path, const QString& key) {
  QDomDocument doc;
  if (!labwcDocument(path, doc)) {
    return std::nullopt;
  }
  const auto element = fontElement(doc, key.section('/', 0, 0)).firstChildElement(key.section('/', 1, 1));
  return element.isNull() ? std::nullopt : std::optional<QString>(element.text());
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool setLabwcFontValue(const QString& path, const QString& key, const std::optional<QString>& value) {
  QDomDocument doc;
  if (!labwcDocument(path, doc)) {
    return false;
  }
  auto theme = doc.documentElement().firstChildElement("theme");
  auto font = fontElement(doc, key.section('/', 0, 0));
  if (font.isNull()) {
    if (!value) {
      return true;
    }
    font = doc.createElement("font");
    font.setAttribute("place", key.section('/', 0, 0));
    theme.appendChild(font);
  }
  auto element = font.firstChildElement(key.section('/', 1, 1));
  if (!element.isNull()) {
    font.removeChild(element);
  }
  if (value) {
    element = doc.createElement(key.section('/', 1, 1));
    element.appendChild(doc.createTextNode(*value));
    font.appendChild(element);
  }
  if (!font.hasChildNodes()) {
    theme.removeChild(font);
  }
  return setFileValue(path, doc.toString(2));
}
const QString overrideBegin = "# BEGIN HoloNight appearance\n";
const QString overrideEnd = "# END HoloNight appearance\n";
std::optional<QString> overrideValue(const QString& path) {
  const auto file = fileValue(path);
  if (!file) return std::nullopt;
  const auto begin = file->indexOf(overrideBegin);
  const auto end = file->indexOf(overrideEnd, begin);
  if (begin < 0 || end < 0) return std::nullopt;
  return file->mid(begin, end + overrideEnd.size() - begin);
}
bool setOverrideValue(const QString& path, const std::optional<QString>& value) {
  QString file = fileValue(path).value_or(QString{});
  const auto old = overrideValue(path);
  if ((file.contains(overrideBegin) || file.contains(overrideEnd)) && !old) return false;
  if (old) file.remove(file.indexOf(overrideBegin), old->size());
  if (value) {
    if (!file.isEmpty() && !file.endsWith('\n')) file += '\n';
    file += *value;
  }
  return setFileValue(path, file);
}
// Restore individual unchanged assignments while retaining edits made inside the block.
bool releaseChangedOverride(const QJsonObject& entry) {
  const QString path = entry["target"].toString();
  const auto current = overrideValue(path);
  if (!current) return true;
  const QStringList managed = entry["last"].toString().split('\n');
  QStringList retained;
  for (const auto& line : current->split('\n')) {
    if (line.isEmpty() || line == overrideBegin.trimmed() || line == overrideEnd.trimmed()) continue;
    if (!managed.contains(line)) retained.append(line);
  }
  auto file = fileValue(path).value_or(QString{});
  file.remove(file.indexOf(*current), current->size());
  const auto original = originalValue(entry, std::nullopt);
  if (original) file += *original;
  if (retained.isEmpty()) return setFileValue(path, file);
  if (!file.isEmpty() && !file.endsWith('\n')) file += '\n';
  return setFileValue(path, file + retained.join('\n') + '\n');
}
std::optional<QString> labwcCurrent(const QJsonObject& entry) {
  if (entry["kind"] == "labwc-override") return overrideValue(entry["target"].toString());
  return entry["kind"].toString() == "labwc-file" ? fileValue(entry["target"].toString())
                                                  : labwcFontValue(entry["target"].toString(), entry["key"].toString());
}
bool labwcSet(const QJsonObject& entry, const std::optional<QString>& value) {
  if (entry["kind"] == "labwc-override") return setOverrideValue(entry["target"].toString(), value);
  return entry["kind"].toString() == "labwc-file"
             ? setFileValue(entry["target"].toString(), value)
             : setLabwcFontValue(entry["target"].toString(), entry["key"].toString(), value);
}
bool labwcSelected(QList<Output>& outputs) {
  const QString path = labwcDirectory() + "/rc.xml";
  QDomDocument doc;
  if (!QFileInfo::exists(path)) {
    outputs.append({
        .name = "labwc/selection",
        .status = "delegated",
        .mode = "delegated",
        .diagnostic = "select HoloNight in rc.xml to enable synchronization",
    });
    return false;
  }
  if (!labwcDocument(path, doc)) {
    outputs.append({
        .name = "labwc/selection",
        .status = "conflict",
        .mode = "live",
        .diagnostic = "rc.xml is malformed or unreadable; preserved",
    });
    return false;
  }
  const auto name = doc.documentElement().firstChildElement("theme").firstChildElement("name").text().trimmed();
  outputs.append({
      .name = "labwc/selection",
      .status = name == "HoloNight" ? "applied" : "delegated",
      .mode = "delegated",
      .diagnostic = name == "HoloNight" ? QString{} : "HoloNight is not selected; synchronization is disabled",
  });
  return name == "HoloNight";
}
void labwcOverrides(const QMap<QString, QByteArray>& files, QList<Output>& outputs) {
  auto contents = fileValue(labwcDirectory() + "/themerc-override");
  if (!contents) {
    return;
  }
  const auto block = overrideValue(labwcDirectory() + "/themerc-override");
  if (block) *contents = contents->mid(contents->indexOf(*block) + block->size());
  QStringList overlap;
  const auto theme = QString::fromUtf8(files["themerc"]);
  for (const auto& raw : contents->split('\n')) {
    const auto key = raw.section(':', 0, 0).trimmed();
    if (!raw.contains(':') || key.startsWith('#') || key.startsWith('!')) {
      continue;
    }
    const auto pattern = QRegularExpression(QRegularExpression::wildcardToRegularExpression(key));
    for (const auto& line : theme.split('\n')) {
      if (pattern.match(line.section(':', 0, 0)).hasMatch()) {
        overlap.append(key);
        break;
      }
    }
    if (key == "border.color") {
      overlap.append(key);
    }
  }
  if (!overlap.isEmpty()) {
    outputs.append({
        .name = "labwc/overrides",
        .status = "conflict",
        .mode = "live",
        .diagnostic = "preserved themerc-override overlaps: " + overlap.join(", "),
    });
  }
}
QMap<QString, QString> labwcExpected(const Holonight::ResolvedAppearance& resolved,
                                     QMap<QString, QByteArray>* filesOut = nullptr) {
  auto files = Holonight::Adapters::labwcTheme(Holonight::resolveSemanticAppearance(resolved));
  QMap<QString, QString> expected;
  for (auto it = files.begin(); it != files.end(); ++it) {
    expected["labwc/file/" + it.key()] = QString::fromUtf8(it.value());
  }
  for (const QString& place : {QStringLiteral("ActiveWindow"), QStringLiteral("InactiveWindow"),
                               QStringLiteral("MenuItem"), QStringLiteral("MenuHeader")}) {
    expected["labwc/font/" + place + "/name"] = resolved.ui_font;
    expected["labwc/font/" + place + "/size"] = QString::number(qRound(resolved.ui_font_size * 96.0 / 72.0));
  }
  if (filesOut != nullptr) {
    *filesOut = std::move(files);
  }
  return expected;
}
QMap<QString, QString> labwcExpected(const QString& appearance, QMap<QString, QByteArray>* filesOut = nullptr) {
  const auto loaded = HoloNight::Config::load(appearance.toStdString());
  if (!loaded || !loaded.value.has_value()) {
    return {};
  }
  const auto resolved = Holonight::resolveAppearance(loaded.value->appearance);
  if (!resolved || !resolved.value.has_value()) {
    return {};
  }
  return labwcExpected(*resolved.value, filesOut);
}

QString labwcRecoveryPath() { return statePath() + ".labwc-recovery.json"; }
void finishLabwcRecovery() {
  const auto journal = readJsonObject(labwcRecoveryPath());
  if (journal && !journal->isEmpty()) {
    const QString backup = journal->value("backup").toString();
    const QString stage = journal->value("stage").toString();
    if (!backup.isEmpty()) {
      QDir(backup).removeRecursively();
    }
    if (!stage.isEmpty()) {
      QDir(stage).removeRecursively();
    }
  }
  QFile::remove(labwcRecoveryPath());
}
bool recoverLabwc() {
  const auto journal = readJsonObject(labwcRecoveryPath());
  if (!journal) {
    return false;
  }
  if (journal->isEmpty()) {
    return true;
  }
  const auto state = readJsonObject(statePath());
  if (!state) {
    return false;
  }
  const auto intended = journal->value("entries").toObject();
  const auto committedEntries = state->value("entries").toObject();
  bool committed = true;
  for (auto it = intended.begin(); it != intended.end(); ++it) {
    if (committedEntries[it.key()] != it.value()) {
      committed = false;
    }
  }
  if (!committed) {
    const QString directory = journal->value("directory").toString();
    const QString backup = journal->value("backup").toString();
    if (!QDir(directory).exists() && QDir(backup).exists() && !QDir().rename(backup, directory)) {
      return false;
    }
    const auto before = journal->value("before").toObject();
    for (auto it = before.begin(); it != before.end(); ++it) {
      const auto entry = it.value().toObject();
      const auto current = labwcCurrent(entry);
      const auto original = originalValue(entry, std::nullopt);
      if (current == original) {
        continue;
      }
      if (current != std::optional<QString>(entry["last"].toString()) || !labwcSet(entry, original)) {
        return false;
      }
    }
  }
  finishLabwcRecovery();
  return true;
}
bool isInstalledLabwcFallback(const QString& directory) {
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
    for (auto iterator = fallback.begin(); iterator != fallback.end(); ++iterator) {
      if (fileValue(directory + '/' + iterator.key()) != std::optional<QString>(QString::fromUtf8(iterator.value()))) {
        installedFallback = false;
      }
    }
  }
  return installedFallback;
}
bool ownsLabwcOutputs(const QMap<QString, QString>& expected, const QJsonObject& entries, const QString& directory,
                      const QString& configPath, bool installedFallback, QList<Output>& outputs) {
  // Refuse the whole labwc transaction on any ownership conflict.
  for (auto it = expected.begin(); it != expected.end(); ++it) {
    const bool isFile = it.key().startsWith("labwc/file/");
    const auto old = entries.value(it.key()).toObject();
    const auto current =
        isFile ? fileValue(directory + '/' + it.key().mid(11)) : labwcFontValue(configPath, it.key().mid(11));
    if ((!old.isEmpty() && (old["target"].toString() != (isFile ? directory + '/' + it.key().mid(11) : configPath) ||
                            current != std::optional<QString>(old["last"].toString()))) ||
        (isFile && old.isEmpty() && !installedFallback && QFileInfo::exists(directory + '/' + it.key().mid(11)))) {
      outputs.append({
          .name = it.key(),
          .status = "conflict",
          .mode = "live",
          .diagnostic =
              QStringLiteral("Generated labwc assets preserved: field %1 at %2; expected last-written value [%3], "
                             "actual [%4]. Use Settings Reapply to replace this customization with a backup.")
                  .arg(it.key().mid(11), isFile ? directory + '/' + it.key().mid(11) : configPath,
                       old.isEmpty() ? QStringLiteral("<unowned>") : old["last"].toString(),
                       current.value_or(QStringLiteral("<missing>"))),
      });
      return false;
    }
  }
  return true;
}
bool stageLabwcFiles(const QString& directory, const QMap<QString, QByteArray>& files, const QString& stagePath) {
  const auto existing = QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
  for (const auto& file : existing) {
    if (!file.isFile() || file.isSymLink() || !QFile::copy(file.filePath(), stagePath + '/' + file.fileName())) {
      return false;
    }
  }
  for (auto it = files.begin(); it != files.end(); ++it) {
    if (!setFileValue(stagePath + '/' + it.key(), QString::fromUtf8(it.value()))) {
      return false;
    }
  }
  return true;
}
bool commitLabwcFiles(const QString& directory, const QString& configPath, const QMap<QString, QString>& expected,
                      const QJsonObject& before, const QJsonObject& intended, QTemporaryDir& stage) {
  const QString backup = stage.path() + "-previous";
  if (!writeJsonObject(labwcRecoveryPath(), {
                                                {"entries", intended},
                                                {"before", before},
                                                {"directory", directory},
                                                {"backup", backup},
                                                {"stage", stage.path()},
                                            })) {
    return false;
  }
  const bool exists = QDir(directory).exists();
  if (exists && !QDir().rename(directory, backup)) {
    return false;
  }
  if (!QDir().rename(stage.path(), directory)) {
    if (exists) {
      QDir().rename(backup, directory);
    }
    return false;
  }
  stage.setAutoRemove(false);

  for (auto it = expected.begin(); it != expected.end(); ++it) {
    if (it.key().startsWith("labwc/font/") && !setLabwcFontValue(configPath, it.key().mid(11), it.value())) {
      return false;
    }
  }
  for (auto it = intended.begin(); it != intended.end(); ++it) {
    const auto entry = it.value().toObject();
    if (entry["kind"] == "labwc-override" && !labwcSet(entry, entry["last"].toString())) return false;
  }
  return true;
}
bool recordLabwcChanges(const QMap<QString, QString>& expected, const QString& directory, const QString& configPath,
                        QJsonObject& entries, QList<Undo>& undo, QJsonObject& before, QJsonObject& intended) {
  bool changed = false;
  for (auto it = expected.begin(); it != expected.end(); ++it) {
    bool isFile = it.key().startsWith("labwc/file/");
    QString key = it.key().mid(11);
    QString target = isFile ? directory + '/' + key : configPath;
    const auto current = isFile ? fileValue(target) : labwcFontValue(target, key);
    const auto old = entries.value(it.key()).toObject();
    entries[it.key()] = stateEntry(baseline(old, current), it.value(), isFile ? "labwc-file" : "labwc-font", target,
                                   isFile ? QString{} : key);
    intended[it.key()] = entries[it.key()];
    before[it.key()] =
        stateEntry(current, it.value(), isFile ? "labwc-file" : "labwc-font", target, isFile ? QString{} : key);
    if (current != std::optional<QString>(it.value())) {
      undo.append({
          .kind = isFile ? Undo::Kind::File : Undo::Kind::LabwcFont,
          .target = target,
          .key = isFile ? QString{} : key,
          .value = current,
      });
      changed = true;
    }
  }
  return changed;
}
bool applyLabwc(const Holonight::ResolvedAppearance& appearance, QJsonObject& entries, QList<Output>& outputs,
                QList<Undo>& undo) {
  if (!labwcSelected(outputs)) {
    return !std::ranges::any_of(
        outputs, [](const Output& output) { return output.name == "labwc/selection" && output.status == "conflict"; });
  }
  QMap<QString, QByteArray> files;
  auto expected = labwcExpected(appearance, &files);
  if (!replacePolicy) labwcOverrides(files, outputs);
  const QString directory = labwcThemeDirectory();
  const QString configPath = labwcDirectory() + "/rc.xml";
  if (QFileInfo(configPath).isSymLink()) return false;
  for (auto it = expected.begin(); it != expected.end(); ++it) {
    const auto old = entries.value(it.key()).toObject();
    const QString target = it.key().startsWith("labwc/file/") ? directory + '/' + it.key().mid(11) : configPath;
    if ((!old.isEmpty() && old["target"].toString() != target) || QFileInfo(target).isSymLink()) return false;
  }
  const bool installedFallback = isInstalledLabwcFallback(directory);
  if (!replacePolicy) {
    for (auto it = expected.begin(); it != expected.end();) {
      if (it.key().startsWith("labwc/font/") &&
          manuallyChanged(entries.value(it.key()).toObject(), labwcFontValue(configPath, it.key().mid(11)))) {
        outputs.append({.name = it.key(), .status = "conflict", .mode = "live", .diagnostic = "custom font preserved"});
        it = expected.erase(it);
      } else
        ++it;
    }
  }

  const bool assetsOwned =
      replacePolicy || ownsLabwcOutputs(expected, entries, directory, configPath, installedFallback, outputs);
  if (!assetsOwned) {
    files.clear();
    for (auto it = expected.begin(); it != expected.end();) {
      if (it.key().startsWith("labwc/file/"))
        it = expected.erase(it);
      else
        ++it;
    }
  }
  // Stage every asset, then replace the directory using same-filesystem renames.
  if (!QDir().mkpath(QFileInfo(directory).absolutePath())) {
    return false;
  }
  QTemporaryDir stage(QFileInfo(directory).absolutePath() + "/.holonight-stage-XXXXXX");
  if (!stage.isValid()) {
    return false;
  }
  if (QFileInfo(directory).isSymLink()) {
    return false;
  }
  if (!stageLabwcFiles(directory, files, stage.path())) {
    return false;
  }
  QJsonObject before;
  QJsonObject intended;
  bool changed = recordLabwcChanges(expected, directory, configPath, entries, undo, before, intended);
  const QString overridePath = labwcDirectory() + "/themerc-override";
  const QString overrideId = "labwc/overrides/appearance";
  const auto oldOverride = entries.value(overrideId).toObject();
  const bool overrideConflict = std::ranges::any_of(
      outputs, [](const Output& output) { return output.name == "labwc/overrides" && output.status == "conflict"; });
  if (assetsOwned && (replacePolicy || (!oldOverride.isEmpty() && !overrideConflict))) {
    QString desired = overrideBegin + QString::fromUtf8(files["themerc"]) + overrideEnd;
    const auto current = overrideValue(overridePath);
    if (replacePolicy || !manuallyChanged(oldOverride, current)) {
      if (QFileInfo(overridePath).isSymLink()) return false;
      const auto entry = stateEntry(baseline(oldOverride, current), desired, "labwc-override", overridePath);
      before[overrideId] = stateEntry(current, desired, "labwc-override", overridePath);
      intended[overrideId] = entry;
      entries[overrideId] = entry;
      const bool misplaced =
          replacePolicy && current && !fileValue(overridePath).value_or(QString{}).endsWith(*current);
      if (current != std::optional<QString>(desired) || misplaced) {
        undo.append({.kind = Undo::Kind::LabwcOverride, .target = overridePath, .key = {}, .value = current});
        changed = true;
      }
    }
  }
  if (changed) {
    // Distinguish recovery from an older identical ownership snapshot.
    for (auto it = intended.begin(); it != intended.end(); ++it) {
      auto entry = it.value().toObject();
      entry["transaction"] = stage.path();
      it.value() = entry;
      entries[it.key()] = entry;
    }
  }
  if (changed && !commitLabwcFiles(directory, configPath, expected, before, intended, stage)) {
    return false;
  }
  outputs.append(
      {.name = "labwc/theme", .status = changed ? "applied" : "unchanged", .mode = "live", .diagnostic = {}});
  return true;
}
void reloadLabwc(QList<Output>& outputs) {
  if (!std::ranges::any_of(outputs, [](const Output& output) {
        return (output.name == "labwc/theme" && output.status == "applied") ||
               (output.name.startsWith("labwc/") && output.status == "restored");
      }))
    return;
  auto unavailable = [&outputs] {
    outputs.append(
        {.name = "labwc/reload",
         .status = "unavailable",
         .mode = "live",
         .diagnostic = "generated assets updated; live compositor reload unavailable (LABWC_PID missing or invalid)"});
  };
  // LABWC_PID is supplied by labwc to its session children. Validate ownership and executable.
  bool valid = false;
  const auto pid = qEnvironmentVariable("LABWC_PID").toLongLong(&valid);
  if (!valid || pid <= 1) {
    unavailable();
    return;
  }
  const QString proc = "/proc/" + QString::number(pid);
  if (QFileInfo(proc).ownerId() != static_cast<uint>(getuid()) ||
      QFileInfo(QFile::symLinkTarget(proc + "/exe")).fileName() != "labwc") {
    unavailable();
    return;
  }
  if (::kill(static_cast<pid_t>(pid), SIGHUP) != 0) {
    outputs.append({
        .name = "labwc/reload",
        .status = "unavailable",
        .mode = "live",
        .diagnostic = "reload failed; generated assets remain valid",
    });
  }
}
