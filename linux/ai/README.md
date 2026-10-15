# Family briefing

The native dashboard displays a compact AI briefing below the configured location's weather. The
calendar service, not the UI, holds the OpenAI key and performs HTTPS requests.
It uses `gpt-6-luna` through the Responses API, with reasoning disabled, no tools,
strict JSON output and `store: false`. Availability for a particular API project
must be checked with its real key; the installation makes no paid test request.

## Add or replace the API key from the Mac

Keep the frame connected by USB and booted into its SD Linux system. Run this
from your checked-out project directory in your Mac terminal, with no key or other arguments on the command line:

```sh
python3 linux/ai/set-openai-key.py
```

Paste the key at the hidden prompt and press Return. No characters appear. The
tool verifies the expected USB gadget and waits for a unique no-echo handshake
before transmitting the key. It neither stores the key on the Mac nor records
console output. The fixed-purpose root receiver checks the SD root and absence
of exposed eMMC, encrypts stdin directly with `systemd-creds`, atomically replaces
only `/etc/credstore.encrypted/porthole-openai`, and restarts the calendar service.
It never puts the key in a command argument, environment variable or plaintext
file. Secrets exist transiently in process memory and cross the local USB link.

On the frame, open **Settings → AI**, enable the briefing and choose **Save AI
settings**. The same Mac command replaces the key. **Forget API key** removes the
frame's stored key and cached briefing; it does not revoke the key at OpenAI.
Removing it does not reset the spending record. Host-key encryption protects
ordinary unprivileged access; someone who obtains the entire SD and root access
can potentially recover it. The frame has no TPM-backed protection.

## Data and prompt

An independent calendar fetch keeps today and the next three days available even
while the user browses another calendar month. Payloads contain event title,
start/end, all-day flag, explicit local weekday/date labels and selected calendar name, plus the configured location's daily WMO weather
codes and rounded high/low temperatures. They omit account identifiers, event
UIDs, descriptions, attendees and locations. At most 60 upcoming events and a
bounded input size are included; omitted events are explicitly counted.

Three prompts use the configured local time: morning at 6 AM, midday at noon, and
evening at 4 PM. Morning emphasizes household-specific special lunch days, school events,
and weather for clothing; weekday midday looks ahead three or four days and
increasingly toward weekend plans. Weekend midday focuses on remaining plans and
tomorrow, with Monday preparation on Sunday. Evening emphasizes tonight and
tomorrow's school/lunch facts, early starts, and weather. Important later events
retain their actual weekday instead of being pulled into tomorrow. Household
roles and meal arrangements come from private configuration. Adults handle their
own preparation, and a supplied school meal does not imply packing that meal.
Preparation items must be explicitly supplied. Extra saved family
preferences remain bounded to 240 characters and treated as preference data.
Calendar text cannot authorize tools or actions; there are no tools in the
request. Missing or stale source data blocks generation. No arrangements or
preparation items are invented from missing calendar entries. Daily forecast lows
are not presented as morning temperatures. Briefings use short direct sentences,
without semicolons or generic preparation advice. The response boundary converts
semicolon chains into sentences and rechecks the physical character limit.

API data is not used for training by default. `store: false` disables stored
response application state, but standard abuse monitoring and applicable prompt
caching retention can still apply. See [OpenAI data controls](https://developers.openai.com/api/docs/guides/your-data).

## Physical display limits

New briefings target **140–200 Unicode code points**, with a hard maximum of
**240**, two short sentences, and the two or three most useful facts. The prompt
avoids repeating the forecast's highs/lows. Each event phrase references exactly
one event; facts from separate school calendars cannot share one generated source
segment. Weather and connecting words stay neutral.
Oversized or multi-event segments are rejected, retaining the last valid briefing.
Previously cached briefings up to 400 characters retain their complete text and
source mappings. The 174-pixel panel reserves 142 pixels for the body and colour
key. The font adapts to show the entire bounded text, with no display ellipsis.
Long highlighted legacy text with its colour key, unbroken words, both orientations, and narrow
fixture widths are covered by tests. Cache/source state appears beside the AI
attribution. Demo mode uses fictional data and never makes AI requests.

## Frequency, cache and spending

The app checks locally on half-hour slots from **6 AM through the 9 PM check**,
including new baseline periods at 6 AM, noon, and 4 PM. Within a period it calls
OpenAI only when the bounded calendar/forecast or preferences change. Source
check timestamps and the current minute do not change the fingerprint. Calendar
events leaving the upcoming window are changes. Automatic and manual requests
are blocked overnight; the 9 PM slot allows the normal timer's within-minute
dispatch, then closes at 9:01 PM. No later half-hour slot is eligible.

Every attempt counts toward a durable **31-attempt daily cap**; manual requests
retain a five-minute cooldown. The existing monthly cap and pre-request spending
reservation remain. A baseline may wait for source availability or the cooldown.
The dashboard's ordinary calendar/weather refreshes continue overnight.

The private atomic cache retains text, generation period, source fingerprint,
last check, successful validation time, and the spending record. A same-period
cache successfully checked within 30 minutes loads immediately after a restart
and resumes the schedule. Otherwise, a fresh current-period briefing is requested
once source data is ready and the waking window permits it. Overnight boot shows
the saved briefing and resumes at 6 AM. Failed checks do not renew cache freshness.
Legacy state migrates without resetting expenses, preferences, or attempt counts.

The default monthly app limit is **US$1**, with US$0.25 and US$3 alternatives. A
US$0.01 reservation is saved and fsynced before every request. On a trustworthy
usage response it is reconciled against conservative documented token rates;
ambiguous failures retain the entire reservation. Corrupt or unwritable state
stops paid requests, and a backwards clock cannot reset an allowance. State is
private to the calendar service at `/var/lib/porthole-calendar/briefing.json`.
The limit is local to this app, assumes the documented model prices, and cannot
control other programs using the key or future provider price changes.

Pricing checked October 14, 2026: standard short-context input US$0.10/million,
cache writes US$0.125/million, output US$0.50/million. The app conservatively prices
input at the cache-write rate. It uses a bounded 30,000-byte request and at most
512 output tokens. See [GPT-6 Luna](https://developers.openai.com/api/docs/models/gpt-6-luna).

## Checks and installation

`porthole-briefing-tests` covers request/data boundaries, long responses, Unicode,
physical fit, persisted cooldown/cache and spending limits, missing keys, corrupt
state, redirects and authorization failures. `setup-tests.py` uses a Mac
pseudo-terminal to exercise hidden transfer without a real frame or API key.

`install-briefing.sh` expects the sealed bundle prepared by the project workflow.
It guards the verified SD Linux system, backs up the previous app, calendar
service and credential writer, and restores them if installation fails. It makes
no changes to Android, eMMC, boot firmware, Wi-Fi or rotation.

Today’s weather view also requests sunrise and sunset from the existing
[Open-Meteo forecast API](https://open-meteo.com/en/docs), displaying its configured
local-time values only beside the current weather. These solar times are not
added to the three-day strip or the AI input.

Household names and routines are configured under Settings → AI, and weather
location/timezone under Settings → Data. The private on-frame profile is
separate from source and excluded from safe diagnostics. Household context is
included as bounded preference data in AI requests; it never changes the safety
rules, output limit or spending controls.
