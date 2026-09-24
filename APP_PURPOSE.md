# ClipViewer — App Purpose

## What it is

ClipViewer is a personal, self-hosted video clip sharing service, similar to a minimal
Streamable. A user uploads a video clip. The server prepares it for smooth streaming in the
background. The clip then gets a short shareable link that anyone can open to watch it in a
browser, for as long as the server is running.

The main use case is quickly sharing short clips (for example, gameplay moments) with friends
without going through a large public video platform.

## Core behavior

### Uploading
- A signed-in user picks a video file (mp4, webm, mov, avi or mkv) and uploads it.
- They can give it a name (up to 200 characters) when uploading.
- They can also choose a **start and end time** to trim the video, so only that part is kept.
  Today the server does the trimming during processing.
- The server can enforce a maximum upload size.
- The upload returns right away with the clip's short ID. Processing happens afterward.

### Background processing
- Each upload is queued and processed one at a time. Processing:
  - trims the video, if a start/end time was given
  - converts it to a streaming-friendly format
  - generates a thumbnail image
  - generates a scrub-preview sprite (the small preview frames shown when you hover or drag
    along the timeline)
- Progress (0–100%) is tracked while the clip processes, so the user sees a live progress
  indicator.
- A clip ends up in one of these states: Pending → Processing → Completed, or Error.
- If processing fails, the owner can **retry** it.
- The original file stays available as a download or fallback once it's ready.

### Watching and sharing
- Every clip has a short random ID (e.g. `EtmbA`) used in its share link.
- The watch page shows a video player, the clip's name, author, upload date, duration,
  description, tags and chapters.
- Clips are either **public** (they appear in the main feed) or **unlisted** (only people with
  the link can see them. They're hidden from feeds and appear only in the owner's own listing).

### Browsing
- The home feed shows public clips, newest first, as thumbnail tiles.
- You can filter the feed by user or by tag, and see a list of all tags in use.

### Managing clips (owner only)
- Edit name, description (supports Markdown, e.g. embedded images), tags, chapters (named
  timestamps) and public/unlisted status.
- Delete a clip, which removes it and its media files.

### Accounts
- There is no public sign-up. An administrator creates users.
- Each user signs in with a personal **API key**. The same key also lets scripts and other apps
  upload on the user's behalf without a browser session.
- Users have a role: regular user or **Admin**. Admins can list users, create users and rotate
  (regenerate) users' API keys.
- Each user has a stats page: total clips, processed and still processing, unlisted count,
  total duration, total storage used and latest upload date.

## Server upload interface (for a desktop client)

A desktop app can upload to a running ClipViewer server over HTTP:

- **Upload:** `POST /api/upload?name=<optional name>&startTime=<sec>&endTime=<sec>`
  - Header `X-API-Key: <user's API key>`
  - Header `Content-Type` set to the video's type: `video/mp4`, `video/webm`,
    `video/quicktime`, `video/x-msvideo` or `video/x-matroska`
  - Body: the raw video file bytes (not a multipart form)
  - Response: `202 Accepted` with `{ "jobId", "filename", "videoId" }`
  - `startTime`/`endTime` are optional whole seconds. If they're omitted, the server keeps the
    whole video.
- **Check status / get link:** `GET /api/videos/{videoId}` returns the clip, including
  `processed`, `progress` (0–100) and `status` (`Pending`, `Processing`, `Completed`,
  `Error`). Poll this to show progress.
- **Share link:** the clip's watch page on the server, identified by `videoId`.
- **Edit metadata after upload:** `PUT /api/videos/{videoId}` with
  `{ name, description, unlisted, tags: [..], chapters: [{ startTime, title }] }`.
- **Verify a key / get the user:** `GET /api/auth/me` with the `X-API-Key` header.

## Intended split with the desktop app

The desktop app should do the **trimming locally**. The user previews a video on their machine,
picks start/end points and exports the trimmed clip. The app then uploads only the result to the
server, without `startTime`/`endTime`, so the server just processes it as-is. This saves upload
time and bandwidth, and the user can check the exact cut before sending it.

Nice-to-have desktop behaviors that match the web app:
- Store the server URL and the user's API key.
- Set name, tags, description and public/unlisted at upload time, sent with the edit call right
  after the upload.
- Show upload progress, then server processing progress.
- Copy the share link to the clipboard when done.
- Optionally list the user's own clips and their status.
