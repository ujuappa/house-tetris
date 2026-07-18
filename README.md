# House Tetris

A 3D desktop utility built in Unreal Engine 5 that helps you plan a furniture layout before physically moving anything.

Upload a photo of a real piece of furniture, and the Google Gemini API identifies the item and estimates its real-world dimensions. The application spawns a correctly scaled 3D model in a virtual room, which you can then select, move, rotate, and arrange. Collision checks keep furniture from overlapping other items or passing through walls, and any layout can be saved to disk and reloaded later.

**Author:** Giseok Kwon
**Course:** CSE 499 Senior Project — Brigham Young University–Idaho, Spring 2026

---

## Features

- **AI furniture recognition** — a photograph is sent to the Gemini API, which returns the item name, category, estimated dimensions, and a shape description.
- **Automatic scaling** — each model is measured at runtime and scaled to the estimated real-world size, so custom meshes of any original size can be dropped in without manual adjustment.
- **Category-based models** — recognised categories map to custom Blender models. Unmapped categories fall back to a correctly scaled box.
- **Collision-aware placement** — new items spawn into free space, and every move is rejected if it would overlap another item or leave the room.
- **Free camera** — five preset viewpoints plus an orbit camera driven by the mouse.
- **Save and load** — layouts are serialised to JSON and restored exactly, including position and rotation.

---

## Controls

| Input | Action |
| --- | --- |
| `Add Furniture` button | Choose a photo and add the recognised item |
| Click a list row | Select that item (highlighted in yellow) |
| `W` `A` `S` `D` | Move the selected item horizontally |
| `Q` / `E` | Lower / raise the selected item |
| `R` | Rotate the selected item 90° |
| `1`–`5` or the on-screen buttons | Jump to a preset camera |
| Right mouse drag | Orbit the camera around the room |
| Scroll wheel | Zoom in and out |
| `Save` / `Load` | Write or restore the layout |

---

## Setup

### Requirements

- Unreal Engine 5.7
- Visual Studio 2022 with the C++ game development workload
- A Google Gemini API key ([Google AI Studio](https://aistudio.google.com))

### 1. Add your API key

The API key is **not** included in this repository. Open `Config/DefaultGame.ini` and replace the placeholder with your own key:

```ini
[HouseTetris]
GeminiAPIKey=YOUR_API_KEY_HERE
```

Without a valid key the application still runs, but adding furniture will report an API error.

### 2. Build

1. Right-click `house_project.uproject` and choose **Generate Visual Studio project files**.
2. Open the generated solution and build with the **Development Editor / Win64** configuration.
3. Launch the editor from Visual Studio, or by opening `house_project.uproject`.

---

## How it works

1. The user picks an image; `HTHUDWidget::OpenFilePicker()` returns the file path.
2. `GeminiAPIManager` encodes the image as base64 and posts it to the Gemini API with a prompt that constrains the response to a fixed JSON schema.
3. The response is parsed into an `FFurnitureDimensions` struct, and the category string is converted to an `EFurnitureCategory` value.
4. `SpawnFurniture()` looks the category up in a `TMap` of meshes and spawns an `AFurnitureActor`.
5. `ApplyMeshAndScale()` measures the mesh bounds, scales it to the estimated size, corrects the pivot, and seats it on the floor.
6. `FindFreeSpawnLocation()` searches outward from the room centre for a position that does not overlap anything.
7. Moves go through `TryMoveTo()`, which clamps to the room bounds and then rejects the move if the axis-aligned bounding boxes intersect.
8. `SaveLayoutToJson()` writes the layout to `Saved/FurnitureLayout.json`; loading respawns each item and restores its saved transform.

Recognised categories are `Sofa`, `Chair`, `Table`, `Bed`, `Storage`, `Appliance`, and `Other`.

---

## Project structure

```
Config/                     Project configuration, including the API key entry
Content/
  blender/                  Custom furniture models exported from Blender
  ThirdPerson/Maps/         The room level
Source/house_project/
  FurnitureActor            Furniture actor: scaling, collision, category colour
  FurnitureDimensions.h     Dimension struct and category enum
  GeminiAPIManager          API request, prompt, and response parsing
  HTCameraPawn              Preset and orbit camera
  HTPlayerController        Selection, movement, and rotation input
  HTHUDWidget               Main control panel
  HTFurnitureRowWidget      One row of the furniture list
  house_projectGameMode     Spawning, mesh lookup, and JSON save/load
```

---

## Notes and limitations

- The room is a fixed 20 m × 20 m space. User-defined room sizes were attempted but caused the camera presets to frame the room incorrectly and introduced collision boundary bugs, so the room was rolled back to a fixed size for stability.
- Physics simulation is deliberately disabled. Furniture is positioned directly and validated with bounding-box tests, which gives more predictable placement than a physics-driven approach for a layout tool.
- Dimensions are AI estimates based on typical sizes for the item type, not measurements of the specific object in the photo.
