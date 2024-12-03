# Building Unassemblize

## Quick Start

### Windows
- Install Visual Studio 2022 with C++ desktop development workload
- Open the project folder in VS 2022
- Select your build configuration (Debug/Release)
- Build Solution (F7)

### macOS
```sh
# Install dependencies
brew install cmake ccache clang-format doxygen graphviz glfw

# Clone and build
git clone https://github.com/OmniBlade/unassemblize.git
cd unassemblize
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### Ubuntu 22.04 or higher
```sh
# Install dependencies
sudo apt-get update && sudo apt-get install -y \
  ccache \
  clang \
  clang-format \
  cmake \
  doxygen \
  git \
  graphviz \
  libglfw3-dev \
  libgl1-mesa-dev \
  xorg-dev

# Clone and build
git clone https://github.com/OmniBlade/unassemblize.git
cd unassemblize
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Docker (CLI Only)
For command-line usage only, you can use our Docker container:
```sh
docker build -t unassemblize .
docker run -v $(pwd):/work unassemblize [OPTIONS] [INPUT]
```

## Usage

```
$ ./unassemblize

unassemblize r6 ~201fc23
    x86 Unassembly tool

Usage:
  unassemblize [OPTION...] positional parameters

 main options:
      --input-type ["auto", "exe", "pdb"]
                                Input file type. Default is 'auto'
      --input2-type ["auto", "exe", "pdb"]
                                Input file 2 type. Default is 'auto'
  -o, --output arg              Filename for single file output. Default is
                                'auto'
      --cmp-output arg          Filename for comparison file output.
                                Default is 'auto'
      --lookahead-limit arg     Max instruction count for trying to find a
                                matching assembler line ahead. Default is
                                20.
      --match-strictness ["lenient", "undecided", "strict"]
                                Assembler matching strictness. If
                                'lenient', then unknown to known/unknown
                                symbol pairs are treated as match. If
                                'strict', then unknown to known/unknown
                                symbol pairs are treated as mismatch.
                                Default is 'undecided'.
      --print-indent-len arg    Character count for indentation of
                                assembler instructions in output report(s).
                                Default is 4.
      --print-asm-len arg       Max character count for assembler
                                instructions in comparison report(s). Text
                                is truncated if longer. Default is 80.
      --print-byte-count arg    Max byte count in comparison report(s).
                                Text is truncated if longer. Default is 11.
      --print-sourcecode-len arg
                                Max character count for source code in
                                comparison report(s). Text is truncated if
                                longer. Default is 80.
      --print-sourceline-len arg
                                Max character count for source line in
                                comparison report(s). Text is truncated if
                                longer. Default is 5.
  -f, --format ["igas", "agas", "masm", "default"]
                                Assembly output format. Default is 'igas'
      --bundle-file-id [1: 1st input file, 2: 2nd input file]
                                Input file used to bundle match results
                                with. Default is 1.
      --bundle-type ["none", "sourcefile", "compiland"]
                                Method used to bundle match results with.
                                Default is 'sourcefile'.
  -c, --config arg              Configuration file describing how to
                                disassemble the input file and containing
                                extra symbol info. Default is 'auto'
      --config2 arg             Configuration file describing how to
                                disassemble the input file and containing
                                extra symbol info. Default is 'auto'
  -s, --start arg               Starting address of a single function to
                                disassemble in hexadecimal notation.
  -e, --end arg                 Ending address of a single function to
                                disassemble in hexadecimal notation.
      --list-sections           Prints a list of sections in the executable
                                then exits.
  -d, --dumpsyms                Dumps symbols stored in a executable or pdb
                                to the config file.
  -v, --verbose                 Verbose output on current state of the
                                program.
  -h, --help                    Displays this help.
  -g, --gui                     Open with graphical user interface
```
