```bash
██╗     ██╗███╗   ██╗██╗  ██╗███████╗██╗  ██╗
██║     ██║████╗  ██║██║ ██╔╝██╔════╝╚██╗██╔╝
██║     ██║██╔██╗ ██║█████╔╝ █████╗   ╚███╔╝
██║     ██║██║╚██╗██║██╔═██╗ ██╔══╝   ██╔██╗
███████╗██║██║ ╚████║██║  ██╗███████╗██╔╝ ██╗
╚══════╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝

Advanced Manga Chapter Link Extractor v2.0
Built with C++17 | Powered by libcurl & gumbo-parser
------------------------------------------------------------

Usage: linkex <URL>
Example: linkex https://demonicscans.org/manga/The-Beginning-After-the-End

Features:
  • Natural chapter sorting (1, 2, 10 instead of 1, 10, 2)
  • Intelligent URL handling (relative & absolute)
  • Comprehensive error handling & logging
  • Progress tracking with visual indicators
  • Safe filename generation
  • Metadata-rich output files
```
## 🔧 Installation

### 📦 Dependencies

- CMake ≥ 3.10  
- C++17 compatible compiler (e.g., g++ ≥ 7)  
- libcurl  
- gumbo-parser  

---

### 🛠️ Install on Debian/Ubuntu-based systems

```bash
# Update package list
sudo apt update

# Install required packages
sudo apt install -y cmake g++ libcurl4-openssl-dev libgumbo-dev git
```

#Build Instructions

```bash
# Clone the repository
git clone https://github.com/sinescode/linkex
cd linkex

# Create a build directory
mkdir build && cd build

# Generate build files
cmake ..

# Build the project
make
```

## ✨ Features

> 🧪 **Made using the DemonicScans site as a reference**

🔹 **Natural Chapter Sorting**  
Sorts chapters like `1, 2, 10` instead of `1, 10, 2` — because logic matters.

🔹 **Intelligent URL Handling**  
Handles both **relative** and **absolute** URLs seamlessly.

🔹 **Robust Error Handling & Logging**  
Catches and logs failures with meaningful messages so you’re never in the dark.

🔹 **Progress Tracking with Visual Indicators**  
Displays real-time progress for a satisfying UX experience in terminal.

🔹 **Safe Filename Generation**  
Sanitizes filenames to ensure compatibility with all OS and filesystems.

🔹 **Metadata-Rich Output Files**  
Output includes useful chapter info, sorted links, and structured formatting for scripting.

---

🛡 Built for **speed**, **stability**, and **automation**.
