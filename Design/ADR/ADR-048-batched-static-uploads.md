# ADR-048 — Static uploads are batched, and static buffers share default-heap buffers

Status: **accepted** · 2026-10-03 · supersedes [ADR-011](ADR-011-meshes-and-shading.md) decision 5

## Context

ADR-011 decision 5 has `Renderer::CreateStaticBuffer` upload each buffer through an upload buffer of its own and wait for the copy. `CreateStaticTexture` does the same. Each call creates a committed default-heap resource and a committed upload resource, each its own implicit heap of at least 64 KB. It also creates a command allocator and a command list, submits them, and drains the queue.

Loading the game makes about 107 such calls for less than 1 MB of data, counted from the code and the packaged assets:

- four for each of the 23 model pieces: the faces' and the crease lines' vertices and indices;
- ten for the five helper meshes;
- two for the star buffers;
- one for the interface's index buffer;
- the glyph atlas and the particle sprite.

Those are about 107 queue drains and 214 implicit heaps. The data is not what costs; the fixed cost of each call is, and the calls run one after another. This was read from the code, not measured. ADR-049's startup stages measure it.

## Decision

1. **Uploads are batched.** `Renderer::BeginUploads` opens a batch, and `EndUploads` submits it and waits for it once.
   - Inside a batch, `CreateStaticBuffer` and `CreateStaticTexture` record their copies into the renderer's own upload command list.
   - They write their bytes into shared upload buffers of 4 MB, mapped while the batch is open. An upload larger than 4 MB gets one of its own.
   - What they return may not be drawn with before `EndUploads`.
   - Outside a batch, each call is a batch of its own and waits for its copy, as before. The interface re-rasterizing its atlas on a resize still works that way.
   - `WinMain` opens one batch around the client's construction, so every model, star buffer, sprite and the first atlas go to the GPU in one submission.
2. **Static buffers share default-heap buffers.** `CreateStaticBuffer` returns a `StaticBuffer`: the resource that holds the bytes, and their GPU address.
   - Buffers are placed in 4 MB default-heap buffers, each 256-byte aligned. A buffer larger than 4 MB gets one of its own.
   - A view starts at the buffer's address, not at its resource's start. Holding the resource keeps the memory alive.
   - Buffers stay in the common state and are promoted implicitly, as ADR-011 decision 5 had them, so no barrier is recorded.
3. **Textures are still committed resources of their own.** Placing them in shared heaps needs resource heap tier checks and alignment rules. There are only two at load, so that cost buys nothing yet.

## Consequences

- Loading submits once and waits once, instead of about 107 times. It creates a handful of upload and default-heap buffers, instead of about 214 implicit heaps.
- The upload command list belongs to the renderer and is used from the thread that draws. A batch is not thread-safe, and nothing records into one from another thread.
- A mesh's memory is freed only when every mesh in its 4 MB buffer is gone. Nothing unloads meshes today.
- **Forecloses** drawing with a static buffer before its batch is submitted. **Leaves open** placing textures in shared heaps, and streaming uploads on a copy queue while frames are drawn. Structures that grow (ADR-045) should load their levels in the startup batch, not mid-match.
