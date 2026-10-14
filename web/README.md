# Porthole display

A build-free wall display for the 10.1-inch, 16:10 PF1007L photo frame, with **Today**, **Tomorrow**, and **Calendar** tabs. Today and Tomorrow show a giant current clock, current date and weather, and the chosen day's events. Calendar devotes the screen to a month and selected-day overview. Serve this directory at `/` on the same host as the API below. Browser code has no credentials and makes no third-party requests. All dates belong to **UTC**, and weather uses the configured location.

## Dashboard API — version 1

`GET /api/dashboard?month=YYYY-MM` returns UTF-8 JSON. `month` is optional on the first request; default to the current local month. When supplied, return that exact calendar month. Return the whole visible calendar range (Sunday before/on the first through Saturday after/on the last), including events in adjacent-month cells. `upcoming` is independent of the viewed month: return future/today events, sorted chronologically, including ongoing all-day events; omit timed events that have ended. Expand multi-day events into one display occurrence per local day. Never include upstream URLs, credentials, raw ICS, or unnecessary private fields.

```json
{
  "version": 1,
  "clock": {
    "epochMs": 1791900000000,
    "utcOffsetMinutes": -240,
    "localDate": "2026-10-13",
    "dateLabel": "Tuesday, October 13",
    "timeZoneLabel": "Local time"
  },
  "calendar": {
    "status": "sample",
    "month": "2026-10",
    "updatedLabel": "Sample calendar · iCloud is not connected",
    "events": [
      {
        "id": "sample-1",
        "localDate": "2026-10-13",
        "title": "Sample: library visit",
        "timeLabel": "4:00 PM",
        "sortKey": "16:00",
        "allDay": false,
        "category": "family",
        "calendarLabel": "Family",
        "location": "Example location"
      }
    ]
  },
  "upcoming": [],
  "weather": {
    "status": "sample",
    "updatedLabel": "Sample forecast",
    "locationLabel": "Example City",
    "temperatureLabel": "14°",
    "conditionLabel": "Partly cloudy",
    "detailLabel": "Feels like 12° · Rain 10%",
    "icon": "partly-cloudy",
    "forecast": [
      { "dayLabel": "Wed", "highLabel": "16°", "lowLabel": "8°", "icon": "sunny" },
      { "dayLabel": "Thu", "highLabel": "13°", "lowLabel": "7°", "icon": "rain" },
      { "dayLabel": "Fri", "highLabel": "12°", "lowLabel": "5°", "icon": "cloudy" }
    ]
  }
}
```

- `version`: integer `1`. Unknown versions fail visibly.
- `clock`: required. `epochMs` is the current UTC Unix time in milliseconds; `utcOffsetMinutes` is the configured timezone's offset **at that instant** (UTC−4 is `-240`; UTC−5 is `-300`). `localDate` and labels must agree with this instant. Refresh every minute and on resume to pick up daylight-saving changes. The display advances this clock with elapsed browser time, using UTC date methods and the supplied offset; it never assumes the frame's timezone or `Intl` support. A failed request leaves the last clock advancing with an explicit unverified-time label.
- `calendar.status` and `weather.status`: `ready`, `empty`, `stale`, `unavailable`, or `sample`. Use `empty` only for a successful source with no data, and `stale` for cached data whose source could not be refreshed. `unavailable` must have empty event/forecast arrays. `sample` must contain only clearly labeled fictional content. Never report a disconnected calendar as `empty`.
- `updatedLabel`: concise, host-formatted source freshness (e.g. `Updated 9:41 AM`, `Last updated yesterday, 8:10 PM`). A stale label must state the original update time. The UI prefixes stale/unavailable states itself.
- `calendar.month`: valid `YYYY-MM`, years 1970–9999. `events` and `upcoming`: arrays of normalized display occurrences, at most 500 and 100 respectively; the rail scrolls to show every returned event. Events require `id`, valid `localDate`, `title`, `timeLabel`, `sortKey` (`HH:MM`), `allDay` (boolean), and `category` (`family`, `home`, or `school`). `calendarLabel` and `location` are optional strings. Categories are visual groups, not identities; assign source calendars on the host. For all-day events use `timeLabel: "All day"` and `sortKey: "00:00"`. Titles/locations are rendered as text, never HTML. Keep titles concise; long labels truncate in cells but appear in full in day details.
- `upcoming`: same event shape, sorted by `localDate` then `sortKey`. Supply occurrences separately even when outside the visible month. For `empty`/`unavailable` calendar statuses both event arrays must be empty.
- `weather`: required. `locationLabel`, `temperatureLabel`, `conditionLabel`, `detailLabel`, `updatedLabel`, and `icon` are strings. `forecast` has 0–5 entries with the fields above. Units are Celsius and labels are preformatted by the host. Supported icons: `sunny`, `partly-cloudy`, `cloudy`, `rain`, `snow`, `unknown`. Set `icon: "unknown"` when uncertain; no network icon assets are fetched.
- An HTTP error, timeout (12 seconds), malformed JSON, or invalid contract preserves the last valid screen and labels it stale. With no prior data it shows unavailable states and an unverified local-device clock. No calendar or weather data is stored persistently in the frame/browser. Serve private responses with `Cache-Control: no-store`; authenticate/limit access on the serving host as appropriate.
- A partial upstream failure should still return HTTP 200 with independent source statuses and a valid clock. Polls stay local to `/api/dashboard`; upstream refresh/caching/rate limits are the host's responsibility.

## Preview

Use an existing local static server, or from this directory:

```sh
python3 -m http.server 8765 --bind 127.0.0.1
```

Open `http://127.0.0.1:8765/?demo=1` for **explicit sample mode**, with fictional events and forecast. It uses a fixed example date/time and makes no API requests. `?demo=1&state=empty`, `state=stale`, `state=unavailable`, or `state=loading` previews those states. Without `demo=1`, the page calls the API and never silently substitutes sample appointments. A static server will show the unavailable state; tap **View sample** for the demo or **Refresh** to retry the API.

## Interaction and layout

- **Today** is the default screen. It emphasizes current time, date, configured weather and today's events, including earlier appointments and all-day events from `calendar.events`. It uses the host clock's local date and updates the event list when that date changes. The API contract is unchanged; do not remove past-today events from the month data just because `upcoming` excludes them.
- **Tomorrow** keeps the current clock/date and weather, labels weather **Now in the configured location**, and shows tomorrow's full date and events in the large agenda. Tomorrow is calculated from the host's local date using calendar-day arithmetic, so daylight-saving days remain correct. When tomorrow belongs to a different month, the display requests that month; it does not rely on adjacent-month cells having that day. At midnight the Tomorrow view advances to the new tomorrow.
- **Calendar** shows only the month and day overview. Tap a date to see all its events. Browsing another month selects its first day (or today in the current month). Returning to this tab preserves the browsed month and selected day. Month arrows and **Today** are at least 44-pixel touch targets. Dates have keyboard-accessible buttons and meaningful spoken labels. The tab bar supports Left/Right arrows and Home/End keys; the selected tab and panel are labeled for assistive technology.
- The main screen is designed to fit 1280×800 landscape on the 10.1-inch, 16:10 frame. The clock is approximately 205 pixels tall, current temperature is 112 pixels, and event titles are 34 pixels. Time/weather occupy the left 56%; today's plans occupy the right 44%. The agenda scrolls for busy days, while time and weather stay visible. At narrow widths, time/date and weather precede today's events. Shorter desktop viewports retain page scrolling.
- Sample, source freshness, offline and loading labels stay visible. Weather and calendar fail independently. Browsing another month never displays the old month's appointments under a new heading.

## Compatibility and assets

Plain HTML, CSS and ES5 JavaScript; no build, packages, CDN, fonts, framework, Grid, custom properties, `fetch`, Promise, `Intl`, service worker, or persistent browser storage. Uses HTML tables, floats, media queries, XMLHttpRequest, standard DOM methods, and local SVG weather icons. The system-font stack starts with Android's Roboto and falls back to Arial/sans-serif. JavaScript requires `addEventListener`, `textContent`, JSON and ES5 array methods, available in Android-era WebViews. Transitions and animations are omitted. Generated SVG icons are original project assets.

Desktop Chrome preview verifies layout and interactions, not the actual panel's resolution, touch handling, stock WebView engine, frame clock drift, or screen burn-in behavior. Device verification belongs to the hardware task; test the final host URL on the frame before declaring compatibility complete.

## Verification performed

Chrome checks covered 1280×800 landscape (including a six-week month and five forecast days), 390×844 and 320×740 narrow layouts, month navigation, Today, day details, agenda scrolling, and all source-state previews. Visible controls measured at least 44 pixels tall; narrow layouts had no horizontal overflow. A temporary same-origin API fixture verified HTTP failure, malformed JSON, inconsistent clock dates, a mismatched requested month, a 12-second timeout, recovery on refresh, and an independent weather failure. Existing data survived failed refreshes with an offline label. HTML-shaped event titles rendered as literal text, and long titles and unbroken locations wrapped in the agenda. JavaScript syntax checks passed. No dependencies, device actions, commits, or backend changes were required.

The today-first revision was checked in the in-app browser at 1280×800 and 390×844. The full frame screen fit without page scrolling, including an 11:59 clock, −20° temperature and five forecast days. A 12-event day scrolled independently, with the all-day event first and the last event reachable. A simulated local midnight changed the date and today's event list automatically. Empty, loading, stale and unavailable states were checked again. The backend interface remains version 1.

The three-tab revision was checked at 1280×800, 390×844 and 320×740. Today, Tomorrow and a six-week Calendar all fit the frame without page scrolling. Calendar hid the large clock/weather, preserved the selected day across tab switches, and left source labels clear of the tab bar. Arrow-key tab selection worked, and visible controls measured at least 44 pixels high. A January 31 fixture verified that Tomorrow requested February and showed February 1 events. A simulated midnight advanced Tomorrow's date and events. Switching to Today cancelled a slow calendar-month request and reused valid current-month data. Empty, stale, unavailable and loading states were checked in the new views.
