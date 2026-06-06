# Mounting and protecting the TCS3200 for hot work

This is a non-contact measurement, so the sensor never touches the hot object —
it sits back and reads the glow. Good for survival, but the TCS3200 is still a
little plastic-packaged chip (rated to about 85 °C, and you want to stay well
under that), and a glowing target throws off a lot of radiant heat. So there are
two jobs here: mount it so it can see the target without cooking, and put a
barrier between it and the heat.

## Mounting

A few things that actually matter:

**Stand it back.** Radiant heat drops with the square of distance — move twice as
far away, get a quarter of the heat. So don't crowd the source. The limit on the
far side is the field of view: the glowing spot still has to fill what the sensor
sees, or cooler background leaks into the reading. Find the distance where the
target fills the view and the sensor body stays cool to the touch. Ratio
pyrometry doesn't care about the exact distance, so you've got room to back off.

**Come at it from the side.** Hot air rises. Mount above the source and you're
sitting in the plume — you'll heat-soak fast. Look at the target horizontally, or
angled down from the side, so the rising air goes past the sensor instead of over
it.

**Make it rigid.** The reading depends on aim, so the sensor has to keep pointing
at the same hot spot. Use a solid bracket or arm — nothing that sags or drifts
once it warms up. Damp out any nearby machine vibration.

**Break the heat path through the bracket.** A metal arm bolted to hot gear will
conduct heat straight into the sensor, shield or no shield. Put a thermal break in
— ceramic standoffs, or a G10/phenolic spacer — between the hot structure and the
sensor mount.

## Protection

Heat gets to the sensor three ways: radiation (the glow and its IR), convection
(hot air, smoke), and conduction (through the mount). The barrier has to stop heat
while still letting light through. Stack it like this:

**A heat shield with a peephole.** The main barrier is a polished metal plate
between the source and the sensor, with a small hole drilled on the line of sight.
The shiny face reflects most of the radiant heat away; the hole lets the sensor
see the target. Mount it a little in front of the sensor with an air gap, so it
doesn't just conduct back whatever it soaks up. What to make it from, in order of
how hard you're pushing it:

- **Polished aluminium** — the best reflector (emissivity ~0.04), cheap, light, and
  conducts heat well so it's easy to cool. Soft, though, melts near 660 °C, and
  dulls as it oxidizes — use it when the shield itself stays well under ~400 °C,
  and re-polish if it clouds.
- **Polished stainless (304/316)** — the default for closer or hotter work. Good to
  ~800 °C, shrugs off oxidation and spatter, still reflective when polished.
  Heavier, and conducts less.
- **Nickel- or chrome-plated copper** — for a water-cooled shield: copper moves the
  heat, the bright plating keeps emissivity low and resists oxidation.

Finish matters as much as the metal — keep the hot side mirror-bright (a dull or
sooty face absorbs heat instead of reflecting it), and leave the back dull or
finned so it can shed whatever soaks in. For really brutal heat, several thin
polished sheets with air gaps beat one thick plate.

**A window over the lens.** Behind that aperture, put a heat-resistant window so
hot gas, smoke, and spatter can't reach the sensor. It has to do four things at
once: pass the light the sensor reads (visible and near-IR), survive thermal
shock, take the heat, and resist spatter. Picks, coolest to hottest:

- **Borosilicate (Pyrex)** — passes visible + NIR, low expansion so it handles
  thermal shock, good to ~450 °C continuous. Cheap and everywhere (lab sight
  glass). Fine default.
- **Fused quartz / fused silica** — near-zero expansion, takes ~1000 °C and hard
  thermal shock, broad transmission. The standard pyrometer window; reach for it
  when the glass runs hot or sits close.
- **Sapphire** — extremely hard and basically spatter-proof, good past 1500 °C,
  transmits visible–NIR. The one for molten metal or anything abrasive. Expensive
  and harder to source.

Steer clear of ordinary window/soda-lime glass (cracks from thermal shock), any
plastic (melts), and IR-only optics like germanium or ZnSe — those are opaque to
visible light, so they'd blind a colour sensor. A couple of mm thick is plenty,
and don't clamp the glass hard against metal: give it a thin ceramic-fibre or
graphite gasket and a little radial room so expansion doesn't crack it.

One catch: any window tints the light a bit. Since we calibrate anyway, just
calibrate with the window in place and it washes out. Same goes if you add an
IR-cut filter to clean up the ratio — fit it first, then calibrate.

**Cooling, if the heat is constant.** For a quick reading you can lean on thermal
mass — take the measurement and pull back. For sustained exposure, add cooling:

- **Air purge** — a trickle of compressed air bled across the window and out
  through the aperture. Cools the glass and keeps smoke and spatter off it. Cheap
  and works surprisingly well.
- **Fan or heat sink** on the sensor body for the easy cases.
- **Water-cooled jacket** for furnace-level heat. Overkill on a bench, but it's
  the standard industrial answer.

## Putting it together

From the sensor outward: sensor → air gap → window → air gap → heat shield →
source.

- The window sits in a small ring or bracket right in front of the photodiode
  face, parallel to it, with a little air gap. Don't clamp it tight against the
  chip — leave room for air and expansion.
- The heat shield mounts a bit further out, its peephole lined up with the
  sensor's view and the target.
- If you're purging, feed the air into the space behind the window so it blows out
  through the aperture. Positive pressure keeps junk from drifting back in.
- Keep the sensor on insulating standoffs so nothing conducts heat into the board.
- Tie it all to the rigid mount, then re-check the aim once everything's warm —
  parts shift as they heat up.

A nice extra: glue a small thermistor next to the sensor and let the firmware
watch it. If the sensor's own temperature creeps past ~60 °C, flash a warning or
stop reading and back off. Cheap insurance for an expensive afternoon.

## If you only do three things

1. Back it off, and look from the side — not from above.
2. Polished heat shield with a peephole, plus a quartz or borosilicate window, with
   air gaps between each layer.
3. Calibrate with the window (and any filter) in place.
