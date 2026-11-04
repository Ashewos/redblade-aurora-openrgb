"""Generate EN/FR SVG shortcut maps from the pinned QMK physical layout."""
import argparse
from html import escape
import json
from pathlib import Path
import re
from check_fn_map import ROOT, keymap_layers


def generate(qmk):
    source=qmk/'keyboards/designedbygg/redblade/rbfs/keyboard.json'
    board=json.loads(re.sub(r',\s*([}\]])',r'\1',source.read_text(encoding='utf-8')))
    layout=board['layouts']['LAYOUT_fullsize_extended_iso']['layout']
    keys=keymap_layers()[0]
    leds=json.loads((ROOT/'led-map.json').read_text(encoding='utf-8'))
    assert [x['matrix'] for x in layout]==[x['matrix'] for x in leds]
    assert len(layout)==len(keys)==109 and keys[102]=='MO(1)'
    dest=ROOT/'docs/assets';dest.mkdir(parents=True,exist_ok=True)
    for lang in ('en','fr'):
        fr=lang=='fr'
        parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1100" viewBox="0 0 1600 1100">',
               '<rect width="1600" height="1100" fill="#0c1019"/>',
               '<style>text{font-family:Arial,DejaVu Sans,sans-serif;fill:#e9eef7}.title{font-size:40px;font-weight:700}.sub{font-size:20px;fill:#a5b4c9}.key{font-size:15px;font-weight:700;text-anchor:middle}.card{font-size:23px;font-weight:700}.body{font-size:19px}</style>']
        def text(x,y,value,style='body',fill=None):
            parts.append(f'<text x="{x}" y="{y}" class="{style}"'+(f' style="fill:{fill}"' if fill else '')+'>'+escape(value)+'</text>')
        def rect(x,y,w,h,color='#1c2637',stroke='#3b4b63',radius=8):
            parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{color}" stroke="{stroke}" stroke-width="2"/>')
        text(48,62,'RedBlade+ 0.6  /  OpenRGB Firmware','title')
        text(48,98,'Raccourcis + touches dynamiques · positions AZERTY' if fr else 'Shortcuts + dynamic key lighting · AZERTY positions','sub')
        text(48,128,'Carte des fonctions : ces couleurs ne sont pas toutes actives simultanément.' if fr else 'Function map: these colors are not all active at the same time.','sub')
        special={0:('ESC','#e13743'),16:('CAL','#4fcde8'),17:('MUTE','#a593ed'),18:('VOL−','#a593ed'),19:('VOL+','#a593ed'),37:('NUM','#49d68b'),61:('CAPS','#49d68b'),97:('WIN','#e13743'),101:('WIN','#e13743'),102:('FN','#4fcde8'),43:('Z','#49d68b'),44:('E','#e13743'),45:('R','#f3bf57'),46:('T','#f3bf57'),65:('F','#e13743'),99:('SPACE','#e13743'),74:('ENTER','#e13743'),40:('−','#f3bf57'),60:('+','#f3bf57'),14:('SCROLL','#f3bf57')}
        aliases={'KC_ESC':'ESC','KC_LCTL':'CTRL','KC_RCTL':'CTRL','KC_LSFT':'SHIFT','KC_RSFT':'SHIFT','KC_LALT':'ALT','KC_RALT':'ALT GR','KC_BSPC':'BACK','KC_TAB':'TAB','KC_PSCR':'PRINT','KC_SCRL':'SCROLL','KC_PAUS':'PAUSE','KC_CAPS':'CAPS','KC_INS':'INS','KC_HOME':'HOME','KC_PGUP':'PGUP','KC_DEL':'DEL','KC_END':'END','KC_PGDN':'PGDN','KC_UP':'↑','KC_LEFT':'←','KC_DOWN':'↓','KC_RGHT':'→','KC_NUM':'NUM','KC_PSLS':'/','KC_PAST':'*','KC_PMNS':'−','KC_PPLS':'+','KC_PENT':'ENTER','KC_PDOT':'.'}
        pad={'KC_NUM','KC_PSLS','KC_PAST','KC_PMNS','KC_PPLS','KC_PENT','KC_PDOT',*(f'KC_P{i}' for i in range(10))}
        for i,(cell,key) in enumerate(zip(layout,keys)):
            x=48+cell['x']*66;y=164+cell['y']*66;w=cell.get('w',1)*66-6;h=cell.get('h',1)*66-6
            azerty={'KC_Q':'A','KC_W':'Z','KC_A':'Q','KC_Z':'W','KC_M':',','KC_SCLN':'M','KC_MINS':')','KC_EQL':'=','KC_LBRC':'^','KC_RBRC':'$','KC_QUOT':'ù','KC_NUHS':'*','KC_NUBS':'< >','KC_COMM':';','KC_DOT':':','KC_SLSH':'!','KC_GRV':'²'}
            label=azerty.get(key,aliases.get(key,key[3:] if key.startswith('KC_') else key))
            if key in pad and key.startswith('KC_P') and key[-1].isdigit():label=key[-1]
            if len(label)>8:label=''
            stroke='#a4b2c8' if key in pad else '#344157'
            if i in special:label,stroke=special[i]
            if i==14:label='SCRL'
            if 1<=i<=9:stroke={1:'#4fcde8',2:'#e9eef7',3:'#e13743',4:'#546074'}.get(i,'#49d68b')
            rect(round(x,2),round(y,2),round(w,2),round(h,2),'#141e2c',stroke)
            text(round(x+w/2,2),round(y+h/2+5,2),label,'key')
        cards=[('Fn + T','Choisir une touche Turbo' if fr else 'Choose a Turbo key','Puis appuyer/relâcher la touche' if fr else 'Then press and release the target','#f3bf57'),
               ('Fn + num − / +','Cadence −5 / +5 · 5–100/s' if fr else 'Rate −5 / +5 · 5–100/s','Réglable pendant le Turbo' if fr else 'Adjust while Turbo is running','#f3bf57'),
               ('Fn + R','25 → 50 → 100 appuis/s' if fr else '25 → 50 → 100 presses/s','Maj : préréglage précédent' if fr else 'Shift: previous preset','#f3bf57'),
               ('Fn + WIN','Verrouiller Windows' if fr else 'Lock Windows keys','Les deux logos deviennent rouges' if fr else 'Both Windows logos turn red','#e13743'),
               ('Fn + CAL','Guide lumineux · 20 secondes' if fr else 'Lighting guide · 20 seconds','Deuxième appui : fermer' if fr else 'Press again to close','#4fcde8'),
               ('Fn + ESC · 2 s','Mode flash' if fr else 'Bootloader / flash mode','Appui court : arrêt + déverrouillage' if fr else 'Short press: stop + unlock','#e13743')]
        for n,(title,line,detail,color) in enumerate(cards):
            x=48+(n%3)*508;y=602+(n//3)*125
            rect(x,y,488,108,'#141e2c',color)
            text(x+18,y+30,title,'card',color);text(x+18,y+59,line);text(x+18,y+87,detail,'sub')
        text(48,880,'FN : bleu repos · cyan enfoncée/guide · ambre choix · rouge Turbo · vert maintien · violet jeu' if fr else 'FN: blue idle · cyan held/guide · amber selecting · red Turbo · green hold · purple Game Mode','body')
        text(48,923,'Sans flux RGB depuis 5 s : CAPS vert si actif · SCROLL ambre si actif' if fr else 'After 5 s without RGB: CAPS green when active · SCROLL amber when active','body')
        text(48,958,'NUM actif : pavé blanc + NUM vert · NUM inactif : pavé blanc très faible' if fr else 'NUM on: white numpad + green NUM · NUM off: very dim white numpad','body')
        text(48,1003,'ÉCHAP arrête le Turbo · Fn + Entrée rejoue la vague rouge · Fn + F1 rend la main à Aurora' if fr else 'ESC stops Turbo · Fn + Enter replays the red wave · Fn + F1 returns control to Aurora','body')
        text(48,1048,'Les états CAPS/NUM/SCROLL respectent le flux RGB, le profil éteint et la suspension USB.' if fr else 'CAPS/NUM/SCROLL fallback respects live RGB, lighting Off and USB suspension.','sub')
        text(48,1081,'Red Blade Full Size ISO · 4 touches multimédias · Les touches normales gardent leur fonction' if fr else 'Red Blade Full Size ISO · 4 media keys · Normal typing and media functions are preserved','sub')
        parts.append('</svg>')
        (dest/('shortcuts.fr.svg' if fr else 'shortcuts.svg')).write_text('\n'.join(parts)+'\n',encoding='utf-8')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--qmk-tree',type=Path,required=True)
    generate(parser.parse_args().qmk_tree)
