# V2: my own linear-light composite of the minimap's translucent colors, from Hud.cpp's constants, to check the frames.
def enc(c):
    c = max(0.0, min(1.0, c))
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
def dec(s):
    return s / 12.92 if s <= 0.04045 else ((s + 0.055) / 1.055) ** 2.4
def hx(lin):
    return '#' + ''.join('%02X' % round(enc(c) * 255) for c in lin)
def over(top, a, under):
    return [t * a + u * (1 - a) for t, u in zip(top, under)]
MAP = (0.02, 0.04, 0.07); MAP_A = 0.95
OWN = (0.35, 0.65, 1.0); ENEMY = (1.0, 0.38, 0.25); PIRATE = (0.78, 0.45, 1.0)
LATTICE = (0.11, 0.13, 0.17)
ground = over(MAP, MAP_A, [0, 0, 0])
print('map ground', hx(ground))
for n, c in (('own', OWN), ('enemy', ENEMY), ('pirate', PIRATE)):
    w = over(c, 0.12, ground)
    print(n, 'wash', hx(w), 'wash under fog .55', hx([x * 0.45 for x in w]), 'under fog .9', hx([x * 0.1 for x in w]),
          'outline@.55 over lattice', hx(over(c, 0.55, LATTICE)), 'outline@.55 over map', hx(over(c, 0.55, ground)))
print('map under fog .55', hx([x * 0.45 for x in ground]))
