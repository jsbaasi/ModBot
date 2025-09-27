  Some prerequisites:
  - sudo apt install zlib1g-dev
  - sudo apt install libssl-dev
  - sudo apt install libopus0
  - Clone libdpp, build from source
  - Wget sqlite amalgamation config version, extract, ./configure, make, make install
  Then:
  - `git clone`
  - Create build dir in root dir
  - Navigate to build, run `cmake ..`
  - Navigate to root, run `cmake --build build`
  - Download sqlite amalgamation, `make` and then `make install`
  - Add token to mytoken.txt in src/ directory

  To build on laptop server:
  - `git fetch` then `git reset --hard origin/main`
  - Get rid of the cmakelists line that specifies the cxx compiler flags
