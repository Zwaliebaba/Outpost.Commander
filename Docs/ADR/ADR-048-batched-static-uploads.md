# ADR-048 — Static uploads are batched, and static buffers share default-heap buffers

Status: **accepted** · 2026-10-03
Owns: static-upload-batches, static-buffer-pages

## Context

Loading the game uploads many small static buffers and a few textures: every model piece's faces and crease lines, the helper meshes, the star buffers, the interface's index buffer, the glyph atlas and the particle sprite.

An upload done on its own has a fixed cost that its data does not. It needs a committed default-heap resource and a committed upload resource, each its own implicit heap of at least 64 KB, and a command allocator and a command list, submitted and then waited for by draining the queue. Done one after another for every buffer, those fixed costs, not the bytes, are what loading spends. This is read from the code; [ADR-049](ADR-049-startup-in-parallel.md)'s startup stages measure it.

## Decision

1. **Uploads are batched.** `Renderer::BeginUploads` opens a batch, and `EndUploads` submits it and waits for it once.
   - Inside a batch, `CreateStaticBuffer` and `CreateStaticTexture` record their copies into the renderer's own upload command list.
   - They write their bytes into shared upload buffers of 4 MB, mapped while the batch is open. An upload larger than 4 MB gets one of its own.
   - What they return may not be drawn with before `EndUploads`.
   - Outside a batch, each call is a batch of its own and waits for its copy. The interface re-rasterizing its atlas on a resize works that way.
   - `WinMain` opens one batch around the client's construction, so every model, star buffer, sprite and the first atlas go to the GPU in one submission.
2. **Static buffers share default-heap buffers.** `CreateStaticBuffer` returns a `StaticBuffer`: the resource that holds the bytes, and their GPU address.
   - Buffers are placed in 4 MB default-heap buffers, each 256-byte aligned. A buffer larger than 4 MB gets one of its own.
   - A view starts at the buffer's address, not at its resource's start. Holding the resource keeps the memory alive.
   - Buffers stay in the common state and are promoted implicitly, so no barrier is recorded.
3. **Textures are committed resources of their own.** Placing them in shared heaps needs resource heap tier checks and alignment rules. There are only two at load, so that cost buys nothing yet.

## Consequences

- Loading submits once and waits once, into a handful of shared upload and default-heap buffers.
- The upload command list belongs to the renderer and is used from the thread that draws. A batch is not thread-safe, and nothing records into one from another thread.
- A mesh's memory is freed only when every mesh in its 4 MB buffer is gone. Nothing unloads meshes.
- Structures that grow ([ADR-045](ADR-045-levels-and-spinning-parts.md)) load their levels in the startup batch, not mid-match.
- **It leaves open** placing textures in shared heaps, and streaming uploads on a copy queue while frames are drawn.

## What this forecloses

- Drawing with a static buffer before its batch is submitted.
