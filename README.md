# TrailHub Kiosk Application

A Windows-based kiosk application for managing sales with persistent inventory, purchase logging, and real-time interaction tracking.

## Features

- **Persistent Inventory Management** - Saves inventory to CSV after each transaction
- **Purchase Logging** - Tracks all successful and failed transactions
- **Interaction Logging** - Records every UI interaction with timestamps
- **Live Console Terminal** - Real-time display of all activities
- **Payment Simulation** - Test different payment scenarios (Approved/Declined/Timeout/Random)
- **Bundle Support** - Optional bundle item with automatic cart management

## Build Instructions

### Prerequisites
- Windows OS
- Microsoft Visual C++ Compiler (MSVC) or MinGW

### Compilation

**Using MSVC:**
```cmd
cl /EHsc /DUNICODE /D_UNICODE KeoskUI.cpp user32.lib gdi32.lib
```

**Using MinGW:**
```cmd
g++ -std=c++17 -municode -O2 -o KeoskUI.exe KeoskUI.cpp -luser32 -lgdi32
```

## Usage

1. Run `KeoskUI.exe`
2. Two windows will open:
   - GUI kiosk interface
   - Console terminal for live logging
3. Interact with the kiosk to:
   - Add items to cart
   - Select payment methods
   - Process transactions
   - View purchase summaries

## Files Created

- `inventory.csv` - Persistent inventory database
- `purchases.log` - Complete transaction history
- `interactions.log` - Detailed interaction log

## Inventory Format

Default inventory includes 6 items:
- Water Bottle 16oz - $1.50
- Trail Mix 3oz - $2.99
- AA Batteries (4 pack) - $5.49
- Trail-Assist Bundle - $9.99
- Repair Kit - $7.25
- Tent Clip Set - $4.99

## License

MIT License - feel free to use and modify as needed.
