// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#pragma once
#include "semanticappearance.h"
#include <QByteArray>
#include <QMap>
namespace Holonight::Adapters {
QMap<QString, QByteArray> labwcTheme(const SemanticAppearance &appearance);
}
