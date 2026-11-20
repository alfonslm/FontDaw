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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include <QQmlContext>
#include <QQmlEngine>

#include "modularity/ioc.h"
#include "ui/tests/mocks/navigationmocks.h"
#include "playback/qml/MuseScore/Playback/mixerchannelitem.h"

using namespace mu::playback;
using SoloMuteState = mu::notation::INotationSoloMuteState::SoloMuteState;

namespace {
//! A strip, and the solo-mute states it sends as the user's (what is stored in the project)
class Strip
{
public:
    explicit Strip(QObject* parent, MixerChannelItem::Type type = MixerChannelItem::Type::PrimaryInstrument)
        : m_item(new MixerChannelItem(parent, type, false /*outputOnly*/, 1 /*trackId*/)),
        item(*m_item)
    {
        QObject::connect(m_item, &MixerChannelItem::soloMuteStateChanged, [this](const SoloMuteState& state) {
            sent = state;
            allSent.push_back(state);
        });
    }

    ~Strip()
    {
        delete m_item;
    }

private:
    MixerChannelItem* m_item = nullptr;

public:
    MixerChannelItem& item;
    std::optional<SoloMuteState> sent;     // the last state sent
    std::vector<SoloMuteState> allSent;    // every state sent, in order
};

//! What the mixer reads back from the engine for a track that a solo elsewhere silences:
//! AudioOutputParams::setControl() copies the engine's "muted", which includes the solo
mu::project::AudioOutputParams engineParams(bool engineMuted)
{
    mu::project::AudioOutputParams params;
    params.muted = engineMuted;
    return params;
}
}


//! The strip finds its services through the QML engine of the object tree it is in, as in the app.
//! Only the navigation controller its keyboard-navigation panel needs is registered: the strip must
//! work on its own state.
class Playback_MixerChannelItemTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_ctx = std::make_shared<muse::modularity::Context>(1);

        auto navigation = std::make_shared<::testing::NiceMock<muse::ui::NavigationControllerMock> >();
        ON_CALL(*navigation, highlightChanged()).WillByDefault(::testing::Return(muse::async::Notification()));
        muse::modularity::ioc(m_ctx)->registerExport<muse::ui::INavigationController>("playback_qml_tests", navigation);

        m_engine = new QQmlEngine();
        auto qmlIoc = new muse::QmlIoCContext(m_engine);
        qmlIoc->ctx = m_ctx;
        m_engine->rootContext()->setContextProperty("ioc_context", QVariant::fromValue(qmlIoc));
        m_parent = new QObject(m_engine);
        QQmlEngine::setContextForObject(m_parent, m_engine->rootContext());
    }

    void TearDown() override
    {
        delete m_engine;
        muse::modularity::removeIoC(m_ctx);
    }

    QObject* m_parent = nullptr;

private:
    QQmlEngine* m_engine = nullptr;
    muse::modularity::ContextPtr m_ctx;
};

// FD-162: a strip built while another track is soloed, then soloed itself. Before the fix the strip
// took the engine's mute as the user's and sent {mute: true} to be stored
TEST_F(Playback_MixerChannelItemTests, AMuteCausedBySoloIsNotSentAsTheUsersMute)
{
    Strip soloed(m_parent);
    soloed.item.loadSoloMuteState({ /*mute*/ false, /*solo*/ true });
    Strip strip(m_parent);
    strip.item.loadSoloMuteState({ /*mute*/ false, /*solo*/ false });
    strip.item.loadOutputParams(engineParams(true)); // the engine silences it for the solo
    applySoloForceMute({ &soloed.item, &strip.item });

    EXPECT_TRUE(strip.item.muted());
    EXPECT_TRUE(strip.item.forceMute());

    strip.item.setSolo(true);

    ASSERT_TRUE(strip.sent.has_value());
    EXPECT_FALSE(strip.sent->mute);
    EXPECT_TRUE(strip.sent->solo);
    for (const SoloMuteState& state : strip.allSent) {
        EXPECT_FALSE(state.mute); // never, not even for a moment
    }
}

// FD-162: the owner's case. After reopening, the soloed strip loads its solo, then the output params
// (which never hold a solo); before the fix they switched the S button off while the solo played on
TEST_F(Playback_MixerChannelItemTests, OutputParamsDoNotClearTheSolo)
{
    Strip strip(m_parent);
    strip.item.loadSoloMuteState({ /*mute*/ false, /*solo*/ true });
    strip.item.loadOutputParams(engineParams(false));

    EXPECT_TRUE(strip.item.solo());
    EXPECT_FALSE(strip.item.muted());
}

// A solo silences the other strips; lifting it lets them play again, and nothing is stored for them
TEST_F(Playback_MixerChannelItemTests, ASoloLiftedUnmutesTheStrip)
{
    Strip soloed(m_parent);
    soloed.item.loadSoloMuteState({ false, true });
    Strip strip(m_parent, MixerChannelItem::Type::Audio); // an audio track is silenced the same way
    strip.item.loadSoloMuteState({ false, false });

    applySoloForceMute({ &soloed.item, &strip.item });
    EXPECT_TRUE(strip.item.muted());
    EXPECT_TRUE(strip.item.forceMute());

    soloed.item.setSolo(false);
    applySoloForceMute({ &soloed.item, &strip.item });
    EXPECT_FALSE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute());
    EXPECT_FALSE(strip.sent.has_value()); // nothing was stored
}

// The user's own mute still works, and is not "forced"
TEST_F(Playback_MixerChannelItemTests, TheUsersMuteIsSentAndShown)
{
    Strip strip(m_parent);
    strip.item.loadSoloMuteState({ false, false });

    strip.item.setMuted(true);
    ASSERT_TRUE(strip.sent.has_value());
    EXPECT_TRUE(strip.sent->mute);
    EXPECT_FALSE(strip.sent->solo);
    EXPECT_TRUE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute());

    strip.item.setMuted(false);
    EXPECT_FALSE(strip.sent->mute);
    EXPECT_FALSE(strip.item.muted());
}

// The user unmutes a track while another is soloed: it shows as silenced by the solo, not as playing
TEST_F(Playback_MixerChannelItemTests, UnmutingDuringASoloElsewhereShowsTheSolosMute)
{
    Strip soloed(m_parent);
    soloed.item.loadSoloMuteState({ false, true });
    Strip strip(m_parent);
    strip.item.loadSoloMuteState({ /*mute*/ true, /*solo*/ false });

    applySoloForceMute({ &soloed.item, &strip.item });
    EXPECT_TRUE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute()); // the user's own mute: M stays usable

    strip.item.setMuted(false);
    applySoloForceMute({ &soloed.item, &strip.item });
    EXPECT_TRUE(strip.item.muted());
    EXPECT_TRUE(strip.item.forceMute());
    EXPECT_FALSE(strip.sent->mute);
}

// The master strip has no solo-mute state: a mute in its params is the user's own, and the user can
// lift it (a forced mute would disable the button)
TEST_F(Playback_MixerChannelItemTests, TheMastersMuteIsTheUsers)
{
    Strip strip(m_parent, MixerChannelItem::Type::Master);
    strip.item.loadOutputParams(engineParams(true));
    EXPECT_TRUE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute());

    strip.item.setMuted(false);
    EXPECT_FALSE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute());
}

// The metronome is off by default and the engine mutes its track; its stored solo-mute state is never
// set. Its M shows the metronome off, stays usable, and a click asks for the metronome to be switched on.
TEST_F(Playback_MixerChannelItemTests, TheMetronomeOffKeepsItsMuteUsable)
{
    Strip soloed(m_parent);
    soloed.item.loadSoloMuteState({ false, true });
    Strip metronome(m_parent, MixerChannelItem::Type::Metronome);
    metronome.item.loadSoloMuteState({ false, false });
    metronome.item.loadOutputParams(engineParams(true));
    applySoloForceMute({ &soloed.item, &metronome.item }); // a solo does not act on the metronome

    EXPECT_TRUE(metronome.item.muted());
    EXPECT_FALSE(metronome.item.forceMute());

    metronome.item.setMuted(false);
    ASSERT_TRUE(metronome.sent.has_value());
    EXPECT_FALSE(metronome.sent->mute);
    EXPECT_FALSE(metronome.item.muted());

    metronome.item.loadEngineMuted(true); // switched off again, e.g. from the toolbar
    EXPECT_TRUE(metronome.item.muted());
    EXPECT_FALSE(metronome.item.forceMute());
}

// The engine also mutes tracks outside a range selection, and chord symbols with playback off. Those
// are not the user's mute and not a solo: the strip shows neither, and M stays usable.
TEST_F(Playback_MixerChannelItemTests, ARangeSelectionDoesNotGreyOutStrips)
{
    Strip strip(m_parent);
    strip.item.loadSoloMuteState({ false, false });
    strip.item.loadOutputParams(engineParams(true));
    strip.item.loadEngineMuted(true);
    applySoloForceMute({ &strip.item });

    EXPECT_FALSE(strip.item.muted());
    EXPECT_FALSE(strip.item.forceMute());
}

// A solo acts on instruments and audio tracks only, not on aux channels or the master
TEST_F(Playback_MixerChannelItemTests, ASoloDoesNotSilenceAuxOrMaster)
{
    Strip soloed(m_parent);
    soloed.item.loadSoloMuteState({ false, true });
    Strip aux(m_parent, MixerChannelItem::Type::Aux);
    aux.item.loadSoloMuteState({ false, false });
    Strip master(m_parent, MixerChannelItem::Type::Master);
    applySoloForceMute({ &soloed.item, &aux.item, &master.item });

    EXPECT_FALSE(aux.item.forceMute());
    EXPECT_FALSE(aux.item.muted());
    EXPECT_FALSE(master.item.forceMute());
}
