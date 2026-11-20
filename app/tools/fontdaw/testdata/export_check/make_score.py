#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# FontDaw — Copyright (C) 2026 FontDaw contributors
#
# Writes score.mscx, the export check's test score (see README.md here):
#   python3 tools/fontdaw/testdata/export_check/make_score.py tools/fontdaw/testdata/export_check/score.mscx

import sys
# events per measure: ('h', 8) hit eighth, ('r', 8) eighth rest, ('r', 4) quarter rest,
# ('f', 8) hit eighth with fermata, ('b',) caesura after the previous chord
M = [
 [('h',8),('r',8)],                                        # 0 pickup (1/4): beat 4, both
 [('h',8),('r',8),('r',4),('h',8),('r',8),('r',4)],        # 1: b1 both, b3 note
 [('r',4),('r',4),('h',8),('r',8),('r',4)],                # 2: b1 click only, b3 both
 [('h',8),('r',8),('r',4),('h',8),('r',8),('r',4)],        # 3: 90 BPM; b1 both, b3 note
 [('h',8),('r',8),('r',4),('h',8),('r',8),('h',8),('r',8)],# 4: rit.; b1 both, b3 note, b4 both
 [('h',8),('r',8),('r',4),('f',8),('r',8),('h',8),('r',8)],# 5: a tempo 90; b1 note, b3 fermata note, b4 both
 [('h',8),('r',8),('h',8),('b',),('r',8),('h',8),('r',8),('r',4)], # 6: b1 note, b2 note + caesura, b3 both
 [('h',8),('r',8),('r',4),('r',2)],                        # 7: b1 both
 None,                                                     # 8: bar rest, end
]
DT = {8:'eighth', 4:'quarter', 2:'half'}
def tempo(bps, text):
    return f'<Tempo><tempo>{bps}</tempo><followText>1</followText><text><sym>metNoteQuarterUp</sym> = {text}</text></Tempo>'
out = ['<?xml version="1.0" encoding="UTF-8"?>\n<museScore version="4.60"><Score><Division>480</Division>',
 '<metaTag name="workTitle">Export check</metaTag>',
 '<Part id="1"><Staff id="1"><StaffType group="percussion"><name>perc5Line</name></StaffType></Staff>'
 '<trackName>Kick</trackName><Instrument id="drumset"><longName>Kick</longName><shortName>Kick</shortName>'
 '<trackName>Kick</trackName><instrumentId>drum.group.set</instrumentId><useDrumset>1</useDrumset><clef>PERC</clef>'
 '<Drum pitch="36"><head>normal</head><line>7</line><voice>0</voice><name>Acoustic Bass Drum</name><stem>2</stem><shortcut>B</shortcut></Drum>'
 '<Channel><controller ctrl="0" value="1"/><program value="0"/></Channel></Instrument></Part>',
'<Staff id="1"><VBox><height>8</height><Text><style>title</style><text>Export check</text></Text></VBox>']
for i, ev in enumerate(M):
    out.append('<Measure len="1/4">' if i == 0 else '<Measure>')
    v = ['<voice>']
    if i == 0:
        v.append('<KeySig><concertKey>0</concertKey></KeySig><TimeSig><sigN>4</sigN><sigD>4</sigD></TimeSig>' + tempo(2, 120))
    if i == 3 or i == 5:
        v.append(tempo(1.5, 90))
    if i == 4:
        v.append('<Spanner type="GradualTempoChange"><GradualTempoChange><tempoChangeType>ritardando</tempoChangeType>'
                 '<beginText>rit.</beginText><continueText>(rit.)</continueText></GradualTempoChange>'
                 '<next><location><measures>1</measures></location></next></Spanner>')
    if i == 5:
        v.append('<Spanner type="GradualTempoChange"><prev><location><measures>-1</measures></location></prev></Spanner>')
    if ev is None:
        v.append('<Rest><durationType>measure</durationType><duration>4/4</duration></Rest><BarLine><subtype>end</subtype></BarLine>')
    else:
        for e in ev:
            if e[0] == 'r':
                v.append(f'<Rest><durationType>{DT[e[1]]}</durationType></Rest>')
            elif e[0] == 'b':
                v.append('<Breath><symbol>caesura</symbol><pause>0.5</pause></Breath>')
            else:
                f = '<Fermata><subtype>fermataAbove</subtype></Fermata>' if e[0] == 'f' else ''
                v.append(f + f'<Chord><durationType>{DT[e[1]]}</durationType><Note><pitch>36</pitch><tpc>14</tpc><head>normal</head></Note></Chord>')
    v.append('</voice>')
    out.append(''.join(v) + '</Measure>')
out.append('</Staff>')
head, body = out[:2], out[2:]
# the part (an excerpt), stored inline as MuseScore can read it, so the per-part export has a part
excerpt = ['<Score><name>Kick</name>'] + body + ['<Tracklist sTrack="0" dstTrack="0"/></Score>']
open(sys.argv[1], 'w').write('\n'.join(head + body + excerpt + ['</Score></museScore>\n']))
