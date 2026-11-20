/*
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * FontDaw
 * The notation-first DAW
 *
 * Copyright (C) 2026 FontDaw contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "appshell/appicontypes.h"

using namespace mu::appshell;

namespace {
struct ResolveCase {
    AppIconVariant preferred;
    bool followSystem;
    Qt::ColorScheme system;
    AppIconVariant expected;
};
}

TEST(AppIconTypesTests, RoundTripStrings)
{
    EXPECT_EQ(appIconVariantToString(AppIconVariant::Dark), "dark");
    EXPECT_EQ(appIconVariantToString(AppIconVariant::Light), "light");

    EXPECT_EQ(appIconVariantFromString(appIconVariantToString(AppIconVariant::Dark)), AppIconVariant::Dark);
    EXPECT_EQ(appIconVariantFromString(appIconVariantToString(AppIconVariant::Light)), AppIconVariant::Light);
}

TEST(AppIconTypesTests, UnknownAndEmptyStringsGiveDark)
{
    EXPECT_EQ(appIconVariantFromString(""), AppIconVariant::Dark);
    EXPECT_EQ(appIconVariantFromString("unknown"), AppIconVariant::Dark);
    EXPECT_EQ(appIconVariantFromString("LIGHT"), AppIconVariant::Dark);
}

TEST(AppIconTypesTests, Defaults)
{
    EXPECT_EQ(DEFAULT_PREFERRED_APP_ICON, AppIconVariant::Dark);
    EXPECT_TRUE(DEFAULT_APP_ICON_FOLLOWS_SYSTEM);
}

TEST(AppIconTypesTests, ResolveAllCombinations)
{
    const std::vector<ResolveCase> cases = {
        // preferred Dark, follow system on
        { AppIconVariant::Dark, true, Qt::ColorScheme::Dark, AppIconVariant::Dark },
        { AppIconVariant::Dark, true, Qt::ColorScheme::Light, AppIconVariant::Light },
        { AppIconVariant::Dark, true, Qt::ColorScheme::Unknown, AppIconVariant::Dark },
        // preferred Dark, follow system off
        { AppIconVariant::Dark, false, Qt::ColorScheme::Dark, AppIconVariant::Dark },
        { AppIconVariant::Dark, false, Qt::ColorScheme::Light, AppIconVariant::Dark },
        { AppIconVariant::Dark, false, Qt::ColorScheme::Unknown, AppIconVariant::Dark },
        // preferred Light, follow system on
        { AppIconVariant::Light, true, Qt::ColorScheme::Dark, AppIconVariant::Dark },
        { AppIconVariant::Light, true, Qt::ColorScheme::Light, AppIconVariant::Light },
        { AppIconVariant::Light, true, Qt::ColorScheme::Unknown, AppIconVariant::Light },
        // preferred Light, follow system off
        { AppIconVariant::Light, false, Qt::ColorScheme::Dark, AppIconVariant::Light },
        { AppIconVariant::Light, false, Qt::ColorScheme::Light, AppIconVariant::Light },
        { AppIconVariant::Light, false, Qt::ColorScheme::Unknown, AppIconVariant::Light },
    };
    ASSERT_EQ(cases.size(), 12u);

    for (size_t i = 0; i < cases.size(); ++i) {
        const ResolveCase& c = cases[i];
        SCOPED_TRACE("case " + std::to_string(i));
        EXPECT_EQ(resolveAppIconVariant(c.preferred, c.followSystem, c.system), c.expected);
    }
}

TEST(AppIconTypesTests, SvgResourcePaths)
{
    EXPECT_STREQ(appIconSvgResource(AppIconVariant::Dark), ":/fontdawicon/fontdaw-icon-dark.svg");
    EXPECT_STREQ(appIconSvgResource(AppIconVariant::Light), ":/fontdawicon/fontdaw-icon-light.svg");
}

TEST(AppIconTypesTests, PngResourcePathsContainVariantAndSize)
{
    EXPECT_EQ(appIconPngResource(AppIconVariant::Dark, 32), ":/fontdawicon/fontdaw-icon-dark_32x32.png");
    EXPECT_EQ(appIconPngResource(AppIconVariant::Light, 256), ":/fontdawicon/fontdaw-icon-light_256x256.png");

    const std::string darkPath = appIconPngResource(AppIconVariant::Dark, 64);
    EXPECT_NE(darkPath.find("dark"), std::string::npos);
    EXPECT_NE(darkPath.find("64x64"), std::string::npos);

    const std::string lightPath = appIconPngResource(AppIconVariant::Light, 128);
    EXPECT_NE(lightPath.find("light"), std::string::npos);
    EXPECT_NE(lightPath.find("128x128"), std::string::npos);
}
