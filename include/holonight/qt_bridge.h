#pragma once

#include <QByteArray>

namespace Holonight {
struct SemanticAppearance;
}

namespace Holonight::Adapters {
[[nodiscard]] QByteArray serializeSemanticAppearance(const SemanticAppearance &appearance);
}
