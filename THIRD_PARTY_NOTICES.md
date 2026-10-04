# Attribution and licensing

## Project foundation and thanks

**AC8 Integrated Mod is developed from [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim).** We thank FletcherMiya and the upstream contributors for developing and sharing the mouse-flight foundation used by this project. Our online learning, control-policy switching, GUI extensions and optional missile integration build on that foundation.

This repository is independently maintained. Attribution does not imply upstream endorsement of our modifications, and upstream copyright and license notices remain in place.

## Licenses

Original integration code and modifications are provided under the root MIT license. Third-party material keeps its own license and copyright notice.

| Component | Origin / license |
|---|---|
| AC8 Mouse Aim base | [FletcherMiya/AC8-Mouse-Aim](https://github.com/FletcherMiya/AC8-Mouse-Aim), inspected source snapshot `0d6e8a7`; upstream LICENSE declares CC0 1.0. Preserve `licenses/MouseAim/LICENSE.txt` |
| MouseFlight reference | [brihernandez/MouseFlight](https://github.com/brihernandez/MouseFlight), MIT, Brian Hernandez; see `licenses/MouseAim/MouseFlight-LICENSE.txt` |
| MinHook / bundled HDE | [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook), BSD-style notices preserved in `native/src/vendor/minhook/LICENSE.txt` and `licenses/MouseAim/MinHook-LICENSE.txt` |
| RE-UE4SS | [UE4SS-RE/RE-UE4SS](https://github.com/UE4SS-RE/RE-UE4SS), MIT, Narknon and contributors; pinned runtime 3.0.1-1152-ge3ba1016. Header snapshots use explicit standard includes and a `std::format` adaptation |

The missile work originated from research around the user-provided **AC8-PropNav v1.1** release, attributed in its supplied reference to **麦糊**. This repository uses the subsequent source-initialization/verification implementation and records that provenance; it does not import the original third-party distribution wholesale or relicense content without an applicable grant.

No ACE COMBAT game executable, mesh, texture, audio, extracted asset package, decryption key, raw save or raw personal flight recording is included. Asset paths, field names, numeric settings and derived response parameters are used to reference the user's own installed game.

The root license does not replace the individual third-party licenses. Preserve this file and all applicable license files in redistributed packages. This is an unofficial community project and is not affiliated with the game publisher or the upstream projects.
