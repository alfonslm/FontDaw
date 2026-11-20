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

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "draw/bufferedpaintprovider.h"
#include "draw/painter.h"

#include "engraving/dom/masterscore.h"
#include "engraving/dom/page.h"
#include "engraving/rendering/iscorerenderer.h"
#include "engraving/tests/utils/scorerw.h"

#include "modularity/ioc.h"

using namespace mu::engraving;

// A multi-page score from the engraving tests; the hook must run once per page, in page order,
// in page view on screen (isMultiPage) and in print (onNewPage between pages)
class Arrangement_PaintPageHookTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const muse::String path = muse::String::fromUtf8(arrangement_test_DATA_ROOT)
                                  + u"/../../engraving/tests/all_elements_data/moonlight.mscx";
        m_score = ScoreRW::readScore(path, true);
        ASSERT_TRUE(m_score);
        m_score->doLayout();
        ASSERT_GE(m_score->pages().size(), 3u);
    }

    void TearDown() override
    {
        delete m_score;
    }

    //! What the hook saw on one page
    struct HookCall {
        const Page* page = nullptr;
        double dx = 0.0;     // the painter's translation when the hook ran
        double dy = 0.0;
    };

    //! Paints with `opt`. `log` gets, in the order they happen, "page <index>" for every hook call
    //! and "new" for every onNewPage call; `calls` the hook's calls.
    void paint(rendering::IScoreRenderer::ScorePaintOptions opt, bool setHook, std::vector<std::string>& log,
               std::vector<HookCall>& calls)
    {
        log.clear();
        calls.clear();
        if (setHook) {
            opt.onPaintPageEnd = [this, &log, &calls](muse::draw::Painter* painter, const Page* page, const RectF&) {
                const auto& pages = m_score->pages();
                const size_t index = std::find(pages.begin(), pages.end(), page) - pages.begin();
                log.push_back("page " + std::to_string(index));
                calls.push_back({ page, painter->worldTransform().dx(), painter->worldTransform().dy() });
            };
        }
        opt.onNewPage = [&log]() { log.push_back("new"); };

        auto provider = std::make_shared<muse::draw::BufferedPaintProvider>();
        muse::draw::Painter painter(provider, "paintpagehooktests");
        renderer()->paintScore(&painter, m_score, opt);
    }

    //! "page 0", "new", "page 1", "new", ... for every page of the score
    std::vector<std::string> expectedLog() const
    {
        std::vector<std::string> log;
        for (size_t i = 0; i < m_score->pages().size(); ++i) {
            if (i > 0) {
                log.push_back("new");
            }
            log.push_back("page " + std::to_string(i));
        }
        return log;
    }

    muse::GlobalInject<rendering::IScoreRenderer> renderer;
    MasterScore* m_score = nullptr;
};

TEST_F(Arrangement_PaintPageHookTests, OnScreenItRunsOncePerPageInOrderInThePagesCoordinates)
{
    rendering::IScoreRenderer::ScorePaintOptions opt;
    opt.isMultiPage = true;
    opt.isPrinting = true; // not printing would run the debug painter, which needs the GUI's configuration
    std::vector<std::string> log;
    std::vector<HookCall> calls;
    paint(opt, true, log, calls);

    // The hook runs on each page, then the next page is announced
    EXPECT_EQ(log, expectedLog());

    // In page view the painter is translated by the page's position (paint.cpp: translate(pagePos))
    ASSERT_EQ(calls.size(), m_score->pages().size());
    bool someOffset = false;
    for (size_t i = 0; i < calls.size(); ++i) {
        EXPECT_EQ(calls[i].page, m_score->pages()[i]);
        EXPECT_NEAR(calls[i].dx, calls[i].page->pos().x(), 1e-6) << i;
        EXPECT_NEAR(calls[i].dy, calls[i].page->pos().y(), 1e-6) << i;
        someOffset = someOffset || calls[i].page->pos().x() != 0.0 || calls[i].page->pos().y() != 0.0;
    }
    EXPECT_TRUE(someOffset); // or the check above proves nothing
}

TEST_F(Arrangement_PaintPageHookTests, InPrintItRunsOncePerPageAndNewPageSeparatesThem)
{
    rendering::IScoreRenderer::ScorePaintOptions opt;
    opt.isMultiPage = false;
    opt.isPrinting = true;
    std::vector<std::string> log;
    std::vector<HookCall> calls;
    paint(opt, true, log, calls);

    EXPECT_EQ(log, expectedLog());

    // Not in page view: every page is painted at the origin, and the hook finds it there
    ASSERT_EQ(calls.size(), m_score->pages().size());
    for (size_t i = 0; i < calls.size(); ++i) {
        EXPECT_EQ(calls[i].page, m_score->pages()[i]);
        EXPECT_DOUBLE_EQ(calls[i].dx, 0.0) << i;
        EXPECT_DOUBLE_EQ(calls[i].dy, 0.0) << i;
    }
}

TEST_F(Arrangement_PaintPageHookTests, WithoutTheHookNothingIsCalledAndPagesStillPaint)
{
    rendering::IScoreRenderer::ScorePaintOptions opt;
    opt.isMultiPage = false;
    opt.isPrinting = true;
    std::vector<std::string> log;
    std::vector<HookCall> calls;
    paint(opt, false, log, calls);

    EXPECT_TRUE(calls.empty());
    // Only the page changes are announced
    EXPECT_EQ(log, std::vector<std::string>(m_score->pages().size() - 1, "new"));
}
