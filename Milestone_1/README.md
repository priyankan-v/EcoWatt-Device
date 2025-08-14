## How to Build and Run

1. Open a terminal in the `Milestone_1` folder.
2. To build the project, run:

   ```sh
   g++ -std=c++17 -O2 -pthread src/*.cpp -o build/main.exe
   ```

3. To run the program, use:

   ```sh
   ./build/main.exe
   ```

This will compile all source files in `src/` and use headers from `include/`. The executable will be placed in the `build/` directory.
