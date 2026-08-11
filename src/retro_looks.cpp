// SPDX-FileCopyrightText: 2026 kirijin <avel.ronin@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "retro_looks.h"
#include <cstring>

// Order defines the tab's grid order. Every row is a COMPLETE photo look:
// the grade applies to the photo AND its blurred background (coherent), the
// frame/grain/vignette layer on top. textureAsset is optional — a look works
// without it (user drops their own assets into ~/.local/share/walltz/overlays).
//     id          display       sat   γ     warm  lift  vig   grain fr  frW  tex        op    blend over
static const LookConfig s_looks[] = {
    { "kodachrome", "Kodachrome", 1.5,  0.98,  0.22, 0.05,  0.0,  0.02, false, 0,  nullptr,         0.0,  13, false },
    { "polaroid",   "Polaroid",   1.0,  0.94, -0.04, 0.18,  0.15, 0.03, true,  3,  nullptr,         0.0,  13, false },
    { "vintage",    "Vintage",    1.2,  1.06,  0.30, 0.00,  0.20, 0.04, true,  1,  nullptr,         0.0,  13, false },
    { "trix",       "Tri-X",      0.0,  1.10,  0.00, 0.06,  0.25, 0.08, true,  1,  nullptr,         0.0,  13, false },
    { "coolfilm",   "Cool Film",  1.1,  1.03, -0.20, 0.08,  0.10, 0.02, false, 0,  nullptr,         0.0,  13, false },
};
static constexpr int LOOK_COUNT = sizeof(s_looks) / sizeof(s_looks[0]);

int retroLookCount()
{
    return LOOK_COUNT;
}

const LookConfig &retroLookConfig(int index)
{
    return s_looks[index >= 0 && index < LOOK_COUNT ? index : 0];
}

const char *retroLookId(int index)
{
    return retroLookConfig(index).id;
}

const char *retroLookName(int index)
{
    return retroLookConfig(index).displayName;
}
