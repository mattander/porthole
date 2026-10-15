# Porthole native Linux app

An original C++/Qt Widgets application for the PF1007L's SD-only Linux system. The clock, weather, and bounded family briefing sit beside the agenda in landscape and stack in portrait. Widgets render without a browser or embedded web engine. Calendar and Settings are compact, borderless icons in the bottom-right corner, with no footer strip or divider. Settings → Data shows the last successful calendar, weather and briefing updates, including stale-weather and sample-data labels. Today and Tomorrow are large, tappable headings above the agenda; the calendar icon opens the month view and returns to the previously selected day view when tapped again. Settings opens Wi-Fi, iCloud, Display, Data, AI, and Errors & debugging panes. Display → Light / Dark switches the complete native UI and saves the appearance choice in `~/.config/porthole/preferences.ini`; this file contains display preferences, not credentials. Sway and the physically verified sensor/touch services handle orientation.

The direct iCloud connector uses HTTPS CalDAV discovery and read-only PROPFIND/REPORT requests. It sends an app-specific password only to the allowed iCloud CalDAV service hosts, validates TLS normally, checks redirects before forwarding authorization, and bounds responses. Settings → Calendars → iCloud opens the native sign-in form, with a touch keyboard, Show password toggle and Remember this account option. Change calendars, Replace password and Forget account provide account management. Remember this account is selected by default. After successful discovery and calendar selection, the separate calendar service saves an encrypted credential using systemd, restores the account and selection after restart, and exposes calendar data to the restricted dashboard through a local Unix socket. Calendar events remain in memory; no event cache is written. See [persistent account storage](../account/README.md) for the permission boundaries and removable-SD security limit.

The app lists readable event calendars before fetching the calendars the owner chooses. It requests server-expanded recurring instances, parses time zones with libical, and respects exclusive all-day end dates. An unexpanded recurrence response is rejected rather than displayed incorrectly. Live iCloud compatibility must be checked with the owner's account; fixtures do not prove it.

Weather comes from [Open-Meteo](https://open-meteo.com/en/docs) for the configured weather location, using a separate request manager without calendar credentials. The clock and calendar use the configured timezone. Network and clock setup are required before authenticating. The calendar service checks every minute, using sync-token/getctag markers to reuse unchanged calendar data where available and falling back to full time-range reports when markers are unsupported. Weather has its own 15-minute refresh timer. Manual refresh, restored connectivity and view/month changes trigger appropriate additional requests. The offline demo explicitly labels fictional events and weather; its clock uses the system's current time.

Apple documents how to [create a dedicated app-specific password](https://support.apple.com/en-ca/102654). Enter it in the native app, never in chat or command arguments. The app does not need the primary Apple Account password.

## Build and test

The interface uses bundled Poppins Regular, SemiBold and Bold fonts, with Noto
Color Emoji for emoji and joined sequences. Qt 6.9 or later is required for the
application emoji-font selection and COLRv1 rendering. Qt resources embed the
font files and their SIL Open Font Licenses in the dashboard binary, so signed
Wi-Fi updates carry the typography without changing system packages. Settings →
Display → Font licences and `porthole --font-licence` show the included licence.
See [font provenance](fonts/README.md).
`--demo --emoji-preview` displays clearly fictional Unicode and emoji examples
for offline rendering checks.

Display → Follow sunrise and sunset switches to light at sunrise and dark at
sunset using the configured weather location and timezone. It is off by default;
choosing Light or Dark turns it off. Valid solar times for up to four forecast
days are saved with the household profile revision, allowing offline transitions
and restart recovery. Missing or expired times retain the current theme and show
a waiting status; location/timezone changes reject the old schedule.
`--demo --preview-follow-sun --preview-settings --preview-tab Display` previews
the setting using clearly fictional solar times.

For persistent operation, install the reviewed account service bundle on the validated SD system. Build against the matching ARMv7 userspace; keep kernel/modules at the verified versions. Dependencies are gcc, make, cmake, qt6-base, qt6-wayland, and libical. The frame only needs the runtime dependencies.

```sh
cmake -S linux/native -B local/native-build -DCMAKE_BUILD_TYPE=Release
cmake --build local/native-build -j2
ctest --test-dir local/native-build --output-on-failure
local/native-build/porthole --self-check
```

Settings → Wi-Fi lists nearby networks through the Linux iwd service, supports personal password-protected and open networks, and provides a masked on-screen password keyboard with case and symbols. Enterprise provisioning is not implemented. Password replies are restricted to the current iwd service owner and the network selected by the user. iwd stores saved network credentials on the SD card in its root-private profile directory; Porthole does not save another copy. See [Linux network setup](../network/README.md).

`--dark` and `--light` override the saved appearance for visual previews. `--demo` explicitly selects fictional calendar/weather data. `--windowed`, `--portrait`, and `--capture PATH` support offline visual validation; capture only operates when `--demo` is also supplied. `--settings` opens the real Settings pane at launch. `--preview-settings` with `--demo` opens an offline Settings preview with explicitly fictional networks; it must not be used on the running frame. Without `--demo`, the initial screen shows disconnected data until authentication succeeds.

The frame installer expects a private, reviewed offline bundle containing matching signed runtime packages, the compiled app, `baseline.txt`, and `files.sha256`. It checks SD root, absence of eMMC, pinned kernel, file integrity, and the complete existing package baseline before installing. The previous browser compositor configuration is saved for rollback. This installer is specific to this already validated unit and is not a general firmware flashing tool.

Tests cover credential destination boundaries, DAV namespaces/property statuses, local/UTC time conversion, daylight-saving changes, all-day and overnight event boundaries, expanded and cancelled recurrence instances, malformed data, and rejection of the primary-password example before a request. UI tests cover both Settings orientations, touch control boundaries and callbacks, keyboard case/symbol entry, masked passwords, and personal Wi-Fi password-length/key validation.


Wi-Fi discovery, power, scan, connect and agent registration use asynchronous D-Bus messages with bounded timeouts. The app caches the daemon's unique bus owner through owner-change notifications for agent authorization. Background discovery pauses while Wi-Fi password entry or connection is pending. Avoid dynamic `QDBusInterface` construction on the UI thread: its introspection can block even if the following method call is asynchronous.

The `wifi-ui-responsiveness` test uses a private D-Bus daemon and a deliberately delayed fake iwd service. It checks both typing during delayed discovery and typing while a connection awaits the password agent, without accessing a real Wi-Fi profile or joining a network. This test also passed on the physical PF1007L; its maximum sampled UI timer gap was 192 ms, including first dialog creation. This establishes event-loop responsiveness under that fixture, not a human touch-input check.

## Agenda and event details

Tap Today or Tomorrow above the agenda to switch its date; these controls also
return from month browsing. The calendar icon in the bottom-right corner toggles the month
view. Event rows are native buttons with full-row touch targets and keyboard
activation. Tapping a row opens a scrollable detail panel with its title, date/time
range, calendar, location and notes, plus a large Close control. All-day ranges
respect the provider's exclusive end date.

Calendar descriptions are bounded without splitting emoji and remain plain text.
Routine status replies exclude notes. A separate, bounded local request loads one
event's details using an opaque occurrence reference; changed/unavailable events
show a retry message. Notes are not included in AI briefing requests.

`--demo --preview-tomorrow` selects the second heading;
`--demo --preview-event-details` opens a fictional event. Demo captures reject
a clipped briefing, overlapping agenda or off-screen controls.

## Hourly precipitation

A compact six-column strip shows the current hour and following five hourly
precipitation chances alongside the existing three-day forecast. Percentages cover
rain and snow; they are not a guarantee that rain starts at a particular minute.
The existing Open-Meteo provider supplies a separate bounded hourly response every
15 minutes. See its [hourly probability and timestamp definitions](https://open-meteo.com/en/docs#hourly-parameter-definition).

Hourly probabilities refer to the interval preceding each returned timestamp.
Requests use Unix timestamps in UTC; labels convert to the configured timezone,
including daylight-saving transitions. Missing, malformed and expired data never
become zero-percent forecasts. A failed refresh retains an available forecast with
a saved-data label; old or incomplete hourly coverage displays unavailable.

## Family briefing

The AI briefing sits below the weather in a fixed-height area. It has a backend
240-character generation limit (usually 140–200 characters) and a separate
pixel-fit renderer for both orientations. It selects the two or three most useful
facts instead of repeating the forecast's highs/lows. Previously cached briefings
up to 400 characters remain readable without discarding their source mappings.
A small key directly beneath tinted phrases pairs calendar colours with readable
names, once per referenced calendar. It hides for plain-text briefings and neutral
multi-calendar phrases. The key wraps; unusually long lists scroll within a
bounded touch area, preserving the briefing panel and full calendar names.
Settings → AI controls priorities, enable/pause, request timing and a local
monthly spending cap. The OpenAI key is entered privately on the Mac and
transferred over USB; see [AI setup](../ai/README.md).

## Approved layout release

The clock paints 12-hour time and AM/PM on a shared baseline. Weather icons cover
today and the next three days; an unknown or invalid forecast keeps the previous
data and records a fixed diagnostic message. The app name and timezone subtitle
are removed. Today/Tomorrow controls sit above the agenda; the calendar icon and Settings
sit in the bottom-right corner in both orientations.

Errors & debugging shows source health, successful update times, available memory,
uptime, conservative AI spending, and up to 50 deduplicated error entries kept in
memory. Retry data uses the existing refresh flow; clearing history removes only
resolved entries. Copy safe diagnostics rebuilds an explicit allowlist, excluding
credentials, account details, Wi-Fi names, and event or briefing contents. Active
issues add a small badge to the Settings gear.

The additional layout suite checks rendered clock baselines, twelve-hour clock
labels, bottom alignment and touch callbacks, weather symbols, bounded diagnostic
history, and sanitized export. The Settings suite also checks diagnostic recovery
and clipboard contents. Existing calendar, authentication, delayed Wi-Fi typing,
and AI privacy/budget tests remain in place.

`install-layout-update.sh` is an SD-only binary update for this configured frame.
It expects the tested app and calendar binaries in `bin/` and `files.sha256`,
preserves the previous binaries, and restarts the dependent orientation watcher
with the kiosk. It does not replace credential helpers or service configuration.

The briefing reserves a 174-pixel panel and a 142-pixel text area. Eight pixels
were moved from the gap above it to accommodate Poppins's larger line height
while retaining the existing screen dimensions and readability floor. It fits
the complete bounded response by adapting its
font rather than appending a display ellipsis. Morning/noon/4 PM prompts follow
weekday/weekend family priorities, with local half-hour change checks from
6 AM through 9 PM, private cache recovery, and a 31-attempt daily cap. See
[AI setup and scheduling](../ai/README.md). Today's weather details also include
sunrise and sunset in the configured local time; forecast tiles remain compact.

## Private household profile

Personal settings are separate from published source. The calendar service owns
`/var/lib/porthole-calendar/profile.json` in its mode-0700 state directory and
writes it atomically with mode 0600. The GUI accesses it through the existing
UID-restricted local socket; it cannot read the file directly. This profile holds
household context, a weather label and coordinates, and an IANA timezone.
Passwords and API keys remain in their existing encrypted credential store.

Settings → Data → Edit location and timezone changes the location and timezone.
Leaving all weather fields empty disables weather; no default location is guessed.
Settings → AI → Edit household context changes names, lunch/school priorities,
and routines. Extra family priorities remain separate. Household context is sent
as preference data, never inserted into the model instructions. Diagnostic exports
omit the profile. Invalid edits retain the previous configuration.

The public `profile.example.json` uses UTC, no household context, and no weather
location. Private `profile.json` files are ignored by Git. Offline demos show a
fictional location and events. Source tests explicitly select regional timezones
to exercise daylight-saving behavior; they are unrelated to an owner's profile.

The schema is version 1 with `timezone`, `familyContext` and `weather` fields.
`weather` is either `null` or an object with a nonempty `label`, numeric `latitude`
(-90 to 90) and numeric `longitude` (-180 to 180). The timezone must be an available
IANA identifier; household context is limited to 1,600 Unicode characters.
Unknown fields and malformed values are rejected. Use `profile.example.json`
as an empty template, and keep your edited copy outside the publishable tree.

For initial provisioning on an already configured frame with the service users
installed, validate the private file first. Run these commands on the frame,
substituting your transferred private file's actual path:

```sh
sudo /usr/local/bin/porthole-calendar --check-profile /path/to/private/profile.json
sudo install -d -o porthole-calendar -g porthole -m700 /var/lib/porthole-calendar
sudo install -o porthole-calendar -g porthole -m600 \
  /path/to/private/profile.json /var/lib/porthole-calendar/profile.json
sudo systemctl restart porthole-calendar
```

Run each step only after the preceding command succeeds. Prefer the Settings
editors for subsequent changes. The restarted backend delivers the changed profile
to the GUI, refreshes its calendar/forecast data and preserves credentials and the
AI spending record. Passwords and API keys do not belong in this JSON file.

Google Workspace calendars use a separate OAuth refresh credential and read-only
Calendar API connector. Settings → Calendars contains independent iCloud and
Google account controls. Both sources feed the sorted agenda and briefing; the
briefing waits for fresh data from every configured source. See
[Google setup](../google/README.md) for the once-only Mac browser sign-in.

## Calendar colours and briefing sources

Agenda bars use the selected calendar's source colour, with its name kept visible.
iCloud colours come from the Apple CalDAV `calendar-color` property; Google uses
the calendar list's background colour. Missing or malformed colours retain the
ordinary agenda accent. Calendar identity is independent of its display name.

The briefing returns bounded JSON text segments referencing short, request-local
event identifiers. The backend validates references against the exact request,
then resolves them to local calendar identities. Provider URLs, private event
UIDs and colour values are not sent to the model. Weather and connecting words
stay neutral. Only short subject noun phrases reference an event and receive its
calendar tint, such as “Alex’s library pickup.” Predicates, timing and sentence
punctuation stay in neutral segments. Different children's subjects reference
their own events even when they share a sentence. Highlighted subjects are bounded
to 60 Unicode codepoints and eight words, with no sentence-ending punctuation.
Missing spaces after sentence punctuation at segment boundaries are inserted
locally without changing decimals or initials within a segment.

The native text renderer applies a soft calendar tint behind a source phrase,
preserving the normal foreground and the existing measured wrapping. It tests
the actual blended background against a 4.5:1 text contrast target in both themes.
Apple RGBA colours are interpreted in their source channel order and composited
before tinting. Tap the briefing to see its phrase-to-calendar source labels;
the same descriptions are exposed to assistive technology. Titles and generated
text remain plain text, never model-supplied markup.

New generated briefings are limited to 240 Unicode codepoints and fitted inside
the fixed panel in both orientations; the renderer still supports 400-character
legacy caches. `--demo --briefing-school-preview` shows fictional school phrases
with separate yellow/purple tints and a blue home-calendar phrase. Invalid segment metadata displays plain
text, while an invalid generated response keeps the last accepted briefing.
Legacy cached briefings remain readable without whole-sentence styling, and their
segment-boundary spacing is repaired. The existing request cooldown, schedule and
budget govern replacement with the subject-only format; settings and spending
history are retained. Cached source mappings
are private alongside the existing briefing cache; local colour updates do not
require another paid request. Request timing and spending limits still apply.
