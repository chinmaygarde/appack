# App Pack

Thoughts and experiments on a modern application packaging and distribution mechanism.

Tarballs or ZIP archives like APKs for packaging applications are unwieldy for a number of reasons:
* To get a truly portable application, there needs to be a platform/architecture specific package, or a “fat” portable package that can be installed on a number of platforms and devices. Platform/architecture specific packages are difficult to create and maintain for package authors while portable packages are unwieldy to transfer and use for end-users.
* When a package is installed, it is hard to exfiltrate the package from the device for redistribution on the user's other devices. This is a feature for gatekeepers like the app stores.
* On device, there is minimal sharing of resources between applications that may or may not trust or know of each other but nonetheless depend on identical resources. This is for stuff like common frameworks and assets.
* On the content distribution side, it  becomes onerous to maintain package stores for older package versions. Foregoing this task risks making content archival efforts more difficult.

“Pack” files describe the structure of an application as an sqlite3 database. The contents of the package are described by blake3 hashes of the actual contents. These hashes form the keys in unspecified content addressable stores (CAS). While optional, pack files typically describe portable packages. “Installation” of pack files is the resolution of content hashes that are actually required. This happens after a capability check of the platform. Client initiated fetches of required contents can be done during installation or at runtime as necessary.

Let’s take an example of a Flutter application. The user fetches a pack file describing the application. The package describes a portable application for iOS and Android that targets armv7, aarch64 and x64. The pack file is only a few kilobytes. The user has an aarch64 iOS device. The content hashes for this configuration are resolved at application installation time. The installer notices that it already has content hashes resolved for the typical Flutter framework resources, possibly because the user has other Flutter applications of the same version already installed. For the rest, it performs client initiated fetches (described below). Since the iOS device uses an APFS filesystem with COW support, it uses `clonefile` to create a COW clone of the assets in the asset store. This saves devices resources and is fast. On devices without COW support, it uses hardlinks for resources that are read-only and copies for the rest.

Resolving content hashes is intentionally underdefined. The content addressable stores could be anywhere. They could be a cloud storage bucket, resources on the local network, an IPFS pin, whatever. In this experimental repo, since the pack file itself is an sqlite3 database, the CAS (also referred to as a repository) is just a table whose keys are the content hashes and the values are `zstd` compressed contents. This makes the package fully self contained and works for a vast majority of simple cases.

The Flutter application author has a fairly daunting task. They have to build and distribute more than a handful of variants of their applications. But packfiles make this easier. Say they are using GitHub actions. They use matrix strategies for each platform and variant. The conclusion of each run is the submission of the variant manifest to a central builder that builds up the canonical portable pack file, and the submission of the variant artifacts to the transient GitHub artifacts registry (the one that expires in 90 days by default). Once all runners for the matrix are successfully run, the central builder queries the content hashes for the artifacts that have been submitted to it and  promotes those artifacts to the configured repository (which could be the packfile itself for simpler cases). `sqlite3` makes this very easy and very little additional machinery needs to be built for ensuring transactional consistency.

By the user, redistribution of the application is simply the transmission of the packfile to the new device. The original device can also serve as a CAS for peer to peer redistribution of the application on the users own devices. If the target device differs significantly from the source (say the new device is an Android device and needs ELF resources that the iOS device doesn’t have), the target can query a list of repositories that are also written in another `sqlite3` table. The repository list isn’t meant to be exhaustive. The device can query additional repositories and each repository is encouraged to publish bloom filters describing their contents. This allows archivists and repository aggregators to function efficiently. Framework authors like Flutter could even maintain their own repositories.


## Prerequisites

* CMake (3.22 or above).
* Git.
* Ninja.
* [Just](https://just.systems/), a task runner.
* A C11 and C++20 compiler.
* [vcpkg](https://vcpkg.io/en/index.html) for package management.
  * Ensure that the `VCPKG_ROOT` environment variable is present and valid.
