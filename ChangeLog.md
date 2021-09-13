# ChangeLog

## [1.2.10.0](https://github.com/commtech/synccom-windows/releases/tag/v1.2.10.0) (09/13/2021)
- Fixed an issue with over/underreading the frame size list when doing many short frames.
- Fixed a BSOD with deleting objects that were never successfully created.

## [1.2.9](https://github.com/commtech/synccom-windows/releases/tag/v1.2.9) (03/15/2021)
- Fixed the INF files so that the wrong version (x86 vs x64) won't get picked.

## [1.2.8](https://github.com/commtech/synccom-windows/releases/tag/v1.2.8) (05/28/2019)
- Fixed a bug with port numbering.

## [1.2.7](https://github.com/commtech/synccom-windows/releases/tag/v1.2.7) (04/04/2019)
- Updated the license to the MIT license.

## [1.2.6](Unreleased)(01/07/2019)
- Updated copyright years.
- Removed WAIT_ON_WRITE. The USB format doesn't support this. As it was never officially implemented, this does not require a major version change.
- Removed the tracking of sent data. This was not used and sometimes caused considerable delays.
- Updated some header file locations.

## [1.2.5](https://github.com/commtech/synccom-windows/releases/tag/v1.2.5) (06/05/2018)
- Modified the includes directory location to match other Commtech repositories.
- Updated examples for includes changes.
- Fixed the FCR not being read correctly.
- Fixed TXEXT accidentally sending XREP instead of TXEXT.
- Fixed FCR getting overwritten when updating the clock.

## [1.2.4](https://github.com/commtech/synccom-windows/releases/tag/v1.2.4) (10/20/2017)
- Changed the custom class GUID to a new GUID.
- Added a class installer to rename ports with their port number.
- Enabled the drivers to 'remember' what number a port is set to.
- Added a tool to allow a user to find a physical device associated with a port number.

## [1.2.3](https://github.com/commtech/synccom-windows/releases/tag/v1.2.3) (08/02/2017)
- Updated driver project files for specific Windows 10 releases.

## [1.2.2](https://github.com/commtech/synccom-windows/releases/tag/v1.2.2) (04/12/2017)
- Updated driver project for new vendor ID. API remains compatible, but going forward all Sync Com devices will not work without the correct VID.
- Updates to the documentation.

## [1.2.1](https://github.com/commtech/synccom-windows/releases/tag/v1.2.1) (11/18/2016)
- Fixed a bug added by v1.2.0.
- Changed the loopback file for more thorough testing.
- Internal clean up.

## [1.2.0](https://github.com/commtech/synccom-windows/releases/tag/v1.2.0) (11/11/2016)
- Added functionality to check the firmware version of the FX2.
- Fixed the reprogramming widget.
- Cleaned up several files.

## [1.1.2](https://github.com/commtech/synccom-windows/releases/tag/v1.1.2) (09/30/2016)
- Reworked the internals to no longer rely on the ISR.
- Cleaned up the project files and inx files.
- Fixed an issue that caused occasional frame loss or frame delay.

## [1.0.0](https://github.com/commtech/synccom-windows/releases/tag/v1.0.0) (04/21/2016)
This is the initial release of the 1.0 driver series.
