/* Outpost Commander — mesh handoff renderer.
   Every plate in this bundle is rasterised from the vertex arrays in meshes.json by this file.
   Nothing here draws a silhouette by hand: the only 2D drawing is panel labels and dimension
   annotation, which are explicitly marked as annotation and never touch the shapes.
   Shared by "Outpost Commander Meshes.dc.html" and by the frames/*.png export step. */
(function (g) {
  'use strict';

  var D = Math.PI / 180;
  function sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
  function cross(a, b) { return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]; }
  function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  function nrm(v) { var l = Math.hypot(v[0], v[1], v[2]) || 1; return [v[0] / l, v[1] / l, v[2] / l]; }

  function mkCanvas(w, h) {
    if (g.__mkCanvas) return g.__mkCanvas(w, h);
    var c = document.createElement('canvas'); c.width = w; c.height = h; return c;
  }

  function hex2rgb(h) {
    h = h.replace('#', '');
    return [parseInt(h.slice(0, 2), 16), parseInt(h.slice(2, 4), 16), parseInt(h.slice(4, 6), 16)];
  }

  /* rotation: yaw about Y, then pitch about X, then roll about Z (applied R = Ry*Rx*Rz) */
  function rotMat(yaw, pitch, roll) {
    var cy = Math.cos(yaw * D), sy = Math.sin(yaw * D);
    var cp = Math.cos(pitch * D), sp = Math.sin(pitch * D);
    var cr = Math.cos(roll * D), sr = Math.sin(roll * D);
    return [
      cy * cr + sy * sp * sr, -cy * sr + sy * sp * cr, sy * cp,
      cp * sr, cp * cr, -sp,
      -sy * cr + cy * sp * sr, sy * sr + cy * sp * cr, cy * cp
    ];
  }
  function apply(m, v) {
    return [m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
            m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2]];
  }

  /* camera: focus point on the plane, orbited by yaw / pitch-above-plane, at distance dist. */
  function makeCamera(c, aspect) {
    var yaw = (c.yaw || 0) * D, pitch = (c.pitch || 0) * D;
    var focus = c.focus || [0, 0, 0];
    var dist = c.dist || 1000;
    var pos = [focus[0] - Math.sin(yaw) * Math.cos(pitch) * dist,
               focus[1] + Math.sin(pitch) * dist,
               focus[2] - Math.cos(yaw) * Math.cos(pitch) * dist];
    var fwd = nrm(sub(focus, pos));
    var wup = Math.abs(fwd[1]) > 0.999 ? [0, 0, 1] : [0, 1, 0];
    var right = nrm(cross(wup, fwd));
    var up = cross(fwd, right);
    var ortho = c.mode === 'ortho';
    var t = Math.tan((c.vfov || 40) * D / 2);
    var hh = c.halfHeight || 100;
    var zoom = c.zoom || 1, ctr = c.center || [0, 0];
    return {
      pos: pos, fwd: fwd,
      project: function (p) {
        var d = sub(p, pos);
        var vx = dot(d, right), vy = dot(d, up), vz = dot(d, fwd);
        var nx, ny;
        if (ortho) { nx = vx / (hh * aspect); ny = vy / hh; }
        else { if (vz < 1e-4) return null; nx = vx / (vz * t * aspect); ny = vy / (vz * t); }
        nx = (nx - ctr[0]) * zoom; ny = (ny - ctr[1]) * zoom;
        return [nx, ny, vz];
      }
    };
  }

  /* ---- flat-shaded z-buffered triangle rasteriser ---- */
  function rasterise(panel, meshes, mats, W, H) {
    var cam = makeCamera(panel.camera, W / H);
    var zb = new Float32Array(W * H); zb.fill(Infinity);
    var px = new Uint8ClampedArray(W * H * 4);
    var bg = hex2rgb(panel.bg || mats.backdrop.space);
    for (var i = 0; i < W * H; i++) { px[i * 4] = bg[0]; px[i * 4 + 1] = bg[1]; px[i * 4 + 2] = bg[2]; px[i * 4 + 3] = 255; }

    var tones = [hex2rgb(mats.hull.DEEP.hex), hex2rgb(mats.hull.BASE.hex), hex2rgb(mats.hull.EDGE.hex)];
    var key = nrm(mats.light.key.direction), keyI = mats.light.key.intensity;
    var fill = nrm(mats.light.fill.direction), fillI = mats.light.fill.intensity;
    var amb = mats.light.ambient.intensity, ambC = hex2rgb(mats.light.ambient.hex);

    (panel.instances || []).forEach(function (inst) {
      var mesh = meshes.meshes[inst.mesh];
      if (!mesh) return;
      var Rm = rotMat(inst.yaw || 0, inst.pitch || 0, inst.roll || 0);
      var s = inst.scale == null ? 1 : inst.scale;
      var o = inst.pos || [0, 0, 0];
      var teamRGB = inst.team ? hex2rgb(mats.team[inst.team].hex) : tones[1];
      var P = mesh.positions, N = mesh.normals, C = mesh.colors;
      var nTri = mesh.indices.length / 3;
      for (var f = 0; f < nTri; f++) {
        var b = f * 3;
        var sc = [], ok = true;
        for (var k = 0; k < 3; k++) {
          var vi = (b + k) * 3;
          var lp = apply(Rm, [P[vi] * s, P[vi + 1] * s, P[vi + 2] * s]);
          var pr = cam.project([lp[0] + o[0], lp[1] + o[1], lp[2] + o[2]]);
          if (!pr) { ok = false; break; }
          sc.push([(pr[0] * 0.5 + 0.5) * W, (0.5 - pr[1] * 0.5) * H, pr[2]]);
        }
        if (!ok) continue;
        var ni = b * 3;
        var wn = apply(Rm, [N[ni], N[ni + 1], N[ni + 2]]);
        var ci = b * 4, sel = C[ci] / 255, tone = tones[C[ci + 1] === 0 ? 0 : (C[ci + 1] === 255 ? 2 : 1)];
        var alb = [tone[0] + (teamRGB[0] - tone[0]) * sel,
                   tone[1] + (teamRGB[1] - tone[1]) * sel,
                   tone[2] + (teamRGB[2] - tone[2]) * sel];
        var lk = Math.max(0, dot(wn, key)) * keyI, lf = Math.max(0, dot(wn, fill)) * fillI;
        var r = alb[0] * (lk + lf) + alb[0] / 255 * ambC[0] * amb;
        var gg = alb[1] * (lk + lf) + alb[1] / 255 * ambC[1] * amb;
        var bb = alb[2] * (lk + lf) + alb[2] / 255 * ambC[2] * amb;
        fillTri(px, zb, W, H, sc, r, gg, bb);
      }
    });
    return px;
  }

  function fillTri(px, zb, W, H, v, r, g_, b) {
    var x0 = Math.max(0, Math.floor(Math.min(v[0][0], v[1][0], v[2][0])));
    var x1 = Math.min(W - 1, Math.ceil(Math.max(v[0][0], v[1][0], v[2][0])));
    var y0 = Math.max(0, Math.floor(Math.min(v[0][1], v[1][1], v[2][1])));
    var y1 = Math.min(H - 1, Math.ceil(Math.max(v[0][1], v[1][1], v[2][1])));
    if (x1 < x0 || y1 < y0) return;
    var ax = v[0][0], ay = v[0][1], bx = v[1][0], by = v[1][1], cx = v[2][0], cy = v[2][1];
    var den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (Math.abs(den) < 1e-9) return;
    var inv = 1 / den;
    for (var y = y0; y <= y1; y++) {
      for (var x = x0; x <= x1; x++) {
        var pxx = x + 0.5, pyy = y + 0.5;
        var w0 = ((by - cy) * (pxx - cx) + (cx - bx) * (pyy - cy)) * inv;
        var w1 = ((cy - ay) * (pxx - cx) + (ax - cx) * (pyy - cy)) * inv;
        var w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        var z = w0 * v[0][2] + w1 * v[1][2] + w2 * v[2][2];
        var idx = y * W + x;
        if (z >= zb[idx]) continue;
        zb[idx] = z;
        var o = idx * 4;
        px[o] = r; px[o + 1] = g_; px[o + 2] = b; px[o + 3] = 255;
      }
    }
  }

  /* ---- plate compositor: a plate is a canvas of panels, each its own camera + instance list ---- */
  function renderPlate(canvas, plate, meshes, mats) {
    canvas.width = plate.width; canvas.height = plate.height;
    var ctx = canvas.getContext('2d');
    ctx.fillStyle = plate.bg || mats.backdrop.space;
    ctx.fillRect(0, 0, plate.width, plate.height);
    plate.panels.forEach(function (panel) {
      var ss = panel.ss || 2;
      var W = Math.round(panel.w * ss), H = Math.round(panel.h * ss);
      var px = rasterise(panel, meshes, mats, W, H);
      var tmp = mkCanvas(W, H);
      tmp.getContext('2d').putImageData(new ImageData(px, W, H), 0, 0);
      ctx.imageSmoothingEnabled = true;
      if ('imageSmoothingQuality' in ctx) ctx.imageSmoothingQuality = 'high';
      ctx.drawImage(tmp, panel.x, panel.y, panel.w, panel.h);
      if (panel.border !== false) {
        ctx.strokeStyle = 'rgba(130,142,156,0.22)'; ctx.lineWidth = 1;
        ctx.strokeRect(panel.x + 0.5, panel.y + 0.5, panel.w - 1, panel.h - 1);
      }
      if (panel.label) {
        ctx.font = '500 12px ui-monospace, Menlo, Consolas, monospace';
        ctx.fillStyle = 'rgba(186,196,208,0.85)'; ctx.textBaseline = 'top';
        ctx.fillText(panel.label, panel.x + 8, panel.y + 7);
      }
      if (panel.sublabel) {
        ctx.font = '400 11px ui-monospace, Menlo, Consolas, monospace';
        ctx.fillStyle = 'rgba(130,142,156,0.8)'; ctx.textBaseline = 'bottom';
        ctx.fillText(panel.sublabel, panel.x + 8, panel.y + panel.h - 7);
      }
      (panel.annotations || []).forEach(function (a) { annotate(ctx, panel, a); });
      if (panel.overlay) drawOverlay(ctx, panel, mats);
    });
    (plate.captions || []).forEach(function (c) {
      ctx.font = (c.weight || '500') + ' ' + (c.size || 12) + 'px ui-monospace, Menlo, Consolas, monospace';
      ctx.fillStyle = c.color || 'rgba(186,196,208,0.9)';
      ctx.textBaseline = 'top'; ctx.textAlign = c.align || 'left';
      ctx.fillText(c.text, c.x, c.y);
    });
    return canvas;
  }

  /* The fallback the design already named: a shape-coded overlay drawn BY THE INTERFACE,
     in screen space, not by more triangles. Ring = miner, chevron = fighter. */
  var OVERLAY_ROLE = { Scout: 'miner', Frigate: 'fighter' };
  function drawOverlay(ctx, panel, mats) {
    var cam = makeCamera(panel.camera, panel.w / panel.h);
    var r = panel.overlayScale || 7;
    ctx.save();
    ctx.beginPath(); ctx.rect(panel.x, panel.y, panel.w, panel.h); ctx.clip();
    ctx.lineWidth = panel.overlayWeight || 1.25;
    (panel.instances || []).forEach(function (inst) {
      var role = OVERLAY_ROLE[inst.mesh];
      if (!role || !inst.team) return;
      var pr = cam.project(inst.pos || [0, 0, 0]);
      if (!pr) return;
      var x = panel.x + (pr[0] * 0.5 + 0.5) * panel.w, y = panel.y + (0.5 - pr[1] * 0.5) * panel.h;
      ctx.strokeStyle = mats.team[inst.team].hex;
      ctx.globalAlpha = 0.9;
      ctx.beginPath();
      if (role === 'miner') ctx.arc(x, y, r, 0, Math.PI * 2);
      else { ctx.moveTo(x - r, y + r * 0.62); ctx.lineTo(x, y - r * 0.95); ctx.lineTo(x + r, y + r * 0.62); }
      ctx.stroke();
    });
    ctx.restore();
  }

  /* annotation is 2D overlay only — dimension rules over the rendered panel, never geometry */
  function annotate(ctx, panel, a) {
    ctx.save();
    ctx.strokeStyle = 'rgba(255,176,32,0.65)'; ctx.fillStyle = 'rgba(255,176,32,0.95)';
    ctx.lineWidth = 1;
    var x1 = panel.x + a.x1, y1 = panel.y + a.y1, x2 = panel.x + a.x2, y2 = panel.y + a.y2;
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
    var t = 4, dx = x2 - x1, dy = y2 - y1, l = Math.hypot(dx, dy) || 1;
    var nx = -dy / l * t, ny = dx / l * t;
    ctx.beginPath(); ctx.moveTo(x1 - nx, y1 - ny); ctx.lineTo(x1 + nx, y1 + ny);
    ctx.moveTo(x2 - nx, y2 - ny); ctx.lineTo(x2 + nx, y2 + ny); ctx.stroke();
    ctx.font = '500 10px ui-monospace, Menlo, Consolas, monospace';
    ctx.textAlign = 'center'; ctx.textBaseline = 'bottom';
    ctx.fillText(a.text, (x1 + x2) / 2 + (a.tdx || 0), (y1 + y2) / 2 + (a.tdy || -3));
    ctx.restore();
  }

  /* ---------------- camera presets (echoed from the brief; deviations flagged in README) ------- */
  var CAM = {
    tacticalDist: 22500, tacticalPitch: 85, vfov: 40,
    combatDist: 1500, combatPitch: 34,
    upp: 17.06   // units per authored pixel at tactical, 1440-wide authored frame
  };

  /* ---------------- the plates ---------------- */
  function plates(meshes, mats, opts) {
    opts = opts || {};
    var space = mats.backdrop.space;
    /* A panel shorter than the 960-px authored frame would otherwise change units-per-pixel.
       zoom = panelHeight/960 * magnification keeps u/px exactly at the authored figure. */
    var tsz = function (panelH, mag) { return panelH / 960 * (mag || 1); };
    var tac = function (focus, panelH, mag) {
      return { mode: 'persp', dist: CAM.tacticalDist, pitch: CAM.tacticalPitch, yaw: 0,
               vfov: CAM.vfov, focus: focus || [0, 0, 0], zoom: tsz(panelH, mag), center: [0, 0] };
    };
    var com = function (focus, panelH, mag) {
      return { mode: 'persp', dist: CAM.combatDist, pitch: CAM.combatPitch, yaw: 0,
               vfov: CAM.vfov, focus: focus || [0, 0, 0], zoom: tsz(panelH, mag), center: [0, 0] };
    };

    /* --- fleet used by plates 1 and 2 : 9 own, 8 hostile, mixed miners and fighters --- */
    var fleet = [];
    var OWNC = [-5000, 0, 1900], HOSC = [4400, 0, -1900];
    var own = [[-5620, 2430, 'Scout'], [-5180, 2660, 'Scout'], [-4760, 2400, 'Scout'],
               [-5400, 1870, 'Frigate'], [-4880, 1900, 'Frigate'], [-4380, 1780, 'Frigate'],
               [-5860, 1560, 'Scout'], [-4560, 1340, 'Frigate'], [-5060, 1240, 'Scout'],
               [-5100, 2150, 'Cruiser']];
    var hos = [[3860, -1420, 'Frigate'], [4360, -1300, 'Frigate'], [4020, -1960, 'Scout'],
               [4540, -1880, 'Scout'], [4900, -1440, 'Frigate'], [3620, -2000, 'Scout'],
               [4820, -2420, 'Scout'], [4260, -2540, 'Frigate'],
               [4400, -1620, 'Cruiser']];
    own.forEach(function (o, i) { fleet.push({ mesh: o[2], pos: [o[0], 0, o[1]], yaw: 118 + i * 7, team: 'OWN' }); });
    hos.forEach(function (o, i) { fleet.push({ mesh: o[2], pos: [o[0], 0, o[1]], yaw: -58 - i * 9, team: 'B' }); });
    var rocks = [
      { mesh: 'AsteroidE', pos: [-900, 120, -600], yaw: 40, pitch: 22, scale: 1.2 },
      { mesh: 'AsteroidC', pos: [300, -180, 900], yaw: 130, pitch: -40, scale: 1.0 },
      { mesh: 'AsteroidD', pos: [-1800, 60, 1400], yaw: 210, roll: 30, scale: 1.3 },
      { mesh: 'AsteroidB', pos: [1500, 200, -1800], yaw: 300, pitch: 60, scale: 0.9 },
      { mesh: 'AsteroidA', pos: [-300, -90, 2200], yaw: 75, pitch: 10, scale: 1.1 },
      { mesh: 'AsteroidC', pos: [900, 140, -3100], yaw: 12, roll: -35, scale: 0.8 }
    ];

    var P = [];

    /* 1 — acceptance plate: true authored scale, tactical zoom */
    P.push({
      id: '01-tactical-true-scale', width: 1440, height: 960, bg: space,
      title: 'Plate 1 — acceptance. Plan view, tactical zoom, TRUE authored scale (1440 × 960 frame).',
      caption: 'Scout 3.5 px · Frigate 5.3 px · Cruiser 10.6 px · 17.06 units per authored pixel. Nothing is magnified. This is the gate.',
      panels: [{
        x: 0, y: 0, w: 1440, h: 960, ss: 3, border: false, bg: space,
        camera: tac([-300, 0, 0], 960, 1),
        instances: rocks.concat(fleet)
      }],
      captions: [
        { text: 'OWN — 5 miners + 4 fighters + 1 cruiser', x: 330, y: 268, color: mats.team.OWN.hex, size: 12 },
        { text: 'HOSTILE — 4 miners + 4 fighters + 1 cruiser', x: 1000, y: 706, color: mats.team.B.hex, size: 12 },
        { text: 'tactical · d 22,500 · pitch 85° above plane · vfov 40° · 17.06 u/px · 1:1', x: 16, y: 16, color: 'rgba(130,142,156,0.75)', size: 11 }
      ]
    });

    /* 2 — same outlines, magnified 8x */
    P.push({
      id: '02-tactical-mag8', width: 1440, height: 720, bg: space,
      title: 'Plate 2 — the same camera, the same frame, magnified 8×.',
      caption: 'Identical projection to plate 1 — only the NDC window is scaled. What a reviewer is being asked to judge.',
      panels: [
        { x: 0, y: 0, w: 720, h: 720, ss: 3, bg: space, label: 'OWN  ×8',
          sublabel: 'same camera, re-aimed at the formation centre; NDC window ×8',
          camera: tac(OWNC, 720, 8), instances: fleet },
        { x: 720, y: 0, w: 720, h: 720, ss: 3, bg: space, label: 'HOSTILE  ×8',
          sublabel: 'Scout = blunt wedge · Frigate = cruciform spine · Cruiser = hammerhead slab',
          camera: tac(HOSC, 720, 8), instances: fleet }
      ]
    });

    /* 3 — combat view */
    var row = function (z, team, list) {
      return list.map(function (it, i) {
        return { mesh: it[0], pos: [it[1], 0, z], yaw: it[2] || 0, team: team };
      });
    };
    P.push({
      id: '03-combat-34deg', width: 1440, height: 900, bg: space,
      title: 'Plate 3 — combat view, 34° above the plane, d 1,500 (1.14 u/px).',
      caption: 'Scout ≈ 53 px, Frigate ≈ 79 px, Cruiser ≈ 158 px, Station ≈ 193 px. Own team and hostile, same mesh, same draw.',
      panels: [{
        x: 0, y: 0, w: 1440, h: 900, ss: 2, border: false, bg: space,
        camera: { mode: 'persp', dist: 1500, pitch: 34, yaw: 0, vfov: 40, focus: [0, 0, 120],
                  zoom: 900 / 960 },
        instances: [
          { mesh: 'Scout', pos: [-640, 0, 60], yaw: 18, team: 'OWN' },
          { mesh: 'Frigate', pos: [-420, 0, 60], yaw: -12, team: 'OWN' },
          { mesh: 'Cruiser', pos: [-150, 0, 60], yaw: -8, team: 'OWN' },
          { mesh: 'Cruiser', pos: [150, 0, 60], yaw: 188, team: 'B' },
          { mesh: 'Frigate', pos: [420, 0, 60], yaw: 166, team: 'B' },
          { mesh: 'Scout', pos: [640, 0, 60], yaw: 196, team: 'B' },
          { mesh: 'ModuleFrame', pos: [-640, 0, 780], team: 'OWN' },
          { mesh: 'ModuleShipyardL1', pos: [-320, 0, 780], team: 'OWN' },
          { mesh: 'ModuleShipyardL2', pos: [0, 0, 780], team: 'OWN' },
          { mesh: 'ModuleOreProcessorL1', pos: [320, 0, 780], team: 'OWN' },
          { mesh: 'ModuleOreProcessorL2', pos: [640, 0, 780], team: 'OWN' },
          { mesh: 'Station', pos: [0, 0, 1720], team: 'OWN' },
          { mesh: 'AsteroidD', pos: [-980, -60, 1500], yaw: 40, pitch: 25, scale: 1.1 },
          { mesh: 'AsteroidB', pos: [900, 40, 1320], yaw: 200, roll: 40, scale: 0.9 }
        ]
      }],
      captions: [
        { text: 'combat · d 1,500 · pitch 34° · vfov 40° · 1.14 u/px · 1:1', x: 16, y: 16, color: 'rgba(130,142,156,0.75)', size: 11 },
        { text: 'the horizon is off-screen at every pitch ≥ 30°', x: 1424, y: 16, color: 'rgba(130,142,156,0.55)', size: 11, align: 'right' }
      ]
    });

    /* 4 — a base */
    var baseKit = function (team) {
      return [
        { mesh: 'Station', pos: [0, 0, 0], yaw: 0, team: team },
        { mesh: 'ModuleShipyardL1', pos: [-250, 0, 250], yaw: 0, team: team },
        { mesh: 'ModuleShipyardL2', pos: [250, 0, 250], yaw: 0, team: team },
        { mesh: 'ModuleOreProcessorL1', pos: [-250, 0, -250], yaw: 0, team: team },
        { mesh: 'ModuleOreProcessorL2', pos: [250, 0, -250], yaw: 0, team: team }
      ];
    };
    P.push({
      id: '04-base', width: 1440, height: 760, bg: space,
      title: 'Plate 4 — a base: station plus four modules inside the 400-unit radius.',
      caption: 'Left is true tactical scale, 1:1. Right is the same camera ×6. The thing being proved is shipyard-against-processor at ~5 px.',
      panels: [
        { x: 20, y: 40, w: 560, h: 680, ss: 3, bg: space, label: 'TRUE SCALE 1:1',
          sublabel: 'station 12.9 px · module 5.3 px · whole base 47 px',
          camera: tac([0, 0, 0], 680, 1), instances: baseKit('OWN') },
        { x: 600, y: 40, w: 820, h: 680, ss: 3, bg: space, label: 'MAGNIFIED ×6',
          sublabel: 'open gantry = shipyard · closed drum = ore processor · extra lobe = level 2',
          camera: tac([0, 0, 0], 680, 6), instances: baseKit('OWN') }
      ],
      captions: [{ text: 'tactical · plan · 17.06 u/px · module centres at r = 354 u, clear of the 110-u station arm and of each other', x: 20, y: 736, color: 'rgba(130,142,156,0.7)', size: 11 }]
    });

    /* 5 — asteroid field */
    var vNames = ['AsteroidA', 'AsteroidB', 'AsteroidC', 'AsteroidD', 'AsteroidE'];
    var strip = function (scale, seedYaw) {
      return vNames.map(function (n, i) {
        return { mesh: n, pos: [-560 + i * 280, 0, 0], yaw: seedYaw + i * 43, pitch: 18 + i * 31, roll: i * 17, scale: scale };
      });
    };
    var field = [];
    (function () {
      var s = 91711, rand = function () { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
      for (var i = 0; i < 34; i++) {
        field.push({
          mesh: vNames[Math.floor(rand() * 5)],
          pos: [(rand() - 0.5) * 20000, (rand() - 0.5) * 480, (rand() - 0.5) * 6600],
          yaw: rand() * 360, pitch: rand() * 360, roll: rand() * 360,
          scale: 0.75 + rand() * 0.6
        });
      }
    })();
    P.push({
      id: '05-asteroids', width: 1440, height: 1128, bg: space,
      title: 'Plate 5 — five asteroid variants at the extremes of the authored jitter, and a field.',
      caption: 'Scale jitter 0.75–1.35 · yaw/pitch/roll unconstrained · ±240 u off-plane. One draw call per variant.',
      panels: [
        { x: 0, y: 26, w: 1440, h: 300, ss: 2, bg: space, label: 'VARIANTS A–E  ·  scale 0.75 (minimum jitter)',
          camera: com([0, 0, 0], 300, 1), instances: strip(0.75, 24) },
        { x: 0, y: 344, w: 1440, h: 300, ss: 2, bg: space, label: 'VARIANTS A–E  ·  scale 1.35 (maximum jitter)',
          camera: com([0, 0, 0], 300, 1), instances: strip(1.35, 211) },
        { x: 0, y: 662, w: 1440, h: 440, ss: 2, bg: space, label: 'FIELD  ·  tactical zoom, true scale, 34 rocks from 5 variants',
          sublabel: 'rocks are the only thing permitted above and below the plane',
          camera: tac([0, 0, 0], 440, 1), instances: field }
      ]
    });

    /* 6 — orthographic three-views */
    var threeView = [
      ['Scout', 44], ['Frigate', 58], ['Cruiser', 110], ['ModuleFrame', 60], ['Station', 140],
      ['ModuleShipyardL1', 62], ['ModuleShipyardL2', 62],
      ['ModuleOreProcessorL1', 56], ['ModuleOreProcessorL2', 60], ['AsteroidC', 70]
    ];
    var pw = 440, ph = 230, gap = 12, top = 46;
    var panels6 = [];
    threeView.forEach(function (row, r) {
      var m = meshes.meshes[row[0]];
      var y = top + r * (ph + gap);
      var views = [
        { n: 'TOP  (plan, +Z up)', yaw: 0, pitch: 90 },
        { n: 'SIDE (from +X)', yaw: 90, pitch: 0 },
        { n: 'FRONT (from +Z)', yaw: 180, pitch: 0 }
      ];
      views.forEach(function (v, i) {
        panels6.push({
          x: 14 + i * (pw + gap), y: y, w: pw, h: ph, ss: 3, bg: '#080B11',
          label: (i === 0 ? row[0] + '   ' : '') + v.n,
          sublabel: i === 0 ? ('X ' + m.extents.size[0] + '  ·  Y ' + m.extents.size[1] +
                      ' (' + m.extents.min[1] + ' … ' + m.extents.max[1] + ')  ·  Z ' + m.extents.size[2] +
                      '  ·  ' + m.triangles + ' tris') : null,
          camera: { mode: 'ortho', yaw: v.yaw, pitch: v.pitch, halfHeight: row[1],
                    focus: [0, (m.extents.min[1] + m.extents.max[1]) / 2, 0] },
          instances: [{ mesh: row[0], pos: [0, 0, 0], team: 'OWN' }]
        });
      });
    });
    P.push({
      id: '06-ortho-three-views', width: 14 + 3 * pw + 2 * gap + 14, height: top + threeView.length * (ph + gap) + 16,
      bg: '#04060A',
      title: 'Plate 6 — orthographic three-views, authored extents dimensioned.',
      caption: 'Orthographic, no perspective. Extents are read from meshes.json at render time, not typed in.',
      panels: panels6,
      captions: [{ text: 'ORTHOGRAPHIC THREE-VIEWS · world units 1:1 · extents printed from meshes.json', x: 14, y: 18, color: 'rgba(186,196,208,0.8)', size: 12 }]
    });

    /* 7 — the fallback, specified because geometry alone does not clear the gate */
    P.push({
      id: '07-overlay-fallback', width: 1440, height: 800, bg: space,
      title: 'Plate 7 — the fallback: a shape-coded overlay drawn by the interface.',
      caption: 'Screen-space glyphs, constant pixel size, no extra triangles. Ring = miner, chevron = fighter. Specified because plate 1 does not clear the gate on geometry alone.',
      panels: [
        { x: 20, y: 40, w: 680, h: 720, ss: 3, bg: space, label: 'TRUE SCALE 1:1  +  OVERLAY',
          sublabel: 'glyph 14 px across at every zoom — it is a readout, not a hull',
          camera: tac(OWNC, 720, 8), instances: fleet, overlay: true, overlayScale: 7 },
        { x: 720, y: 40, w: 700, h: 720, ss: 3, bg: space, label: 'HOSTILE  ·  same glyph, team-coloured',
          sublabel: 'the glyph carries role; the hull carries owner and heading',
          camera: tac(HOSC, 720, 8), instances: fleet, overlay: true, overlayScale: 7 }
      ],
      captions: [{ text: 'PROPOSAL — not in the MVP readout list. Asked for and approved 2026-09-22; needs a question-register entry before it is built.', x: 20, y: 776, color: 'rgba(255,176,32,0.8)', size: 11 }]
    });

    if (opts.overlayOnAcceptancePlate) {
      P[0].panels[0].overlay = true;
      P[0].panels[0].overlayScale = 9;
      P[1].panels.forEach(function (p) { p.overlay = true; p.overlayScale = 22; p.overlayWeight = 2; });
    }

    return P;
  }

  g.OCRender = { renderPlate: renderPlate, plates: plates, CAM: CAM, makeCamera: makeCamera };
  if (typeof module !== 'undefined' && module.exports) module.exports = g.OCRender;
})(typeof window !== 'undefined' ? window : globalThis);
