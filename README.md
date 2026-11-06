# Kingdom-Terminal-RPG
CLI-based Kingdom Management &amp; Turn-Based RPG built in C++

A CLI text-based RPG built in C++. This project combines kingdom resource management inspired by Slay the Spire with turn-based combat mechanics.

This game was developed to explore and solidify core programming concepts, including complex data structures, state machines, and game loop architecture, all functioning purely on backend logic without relying on a graphical user interface (GUI).

## Gameplay Showcase

<img width="100%" alt="image" src="https://github.com/user-attachments/assets/5c509844-559f-4a86-b9c7-ad8affca872c" />

<img width="100%" alt="image" src="https://github.com/user-attachments/assets/74067236-f30b-4c6b-94ac-984720890c80" />

## Key Features

- Dynamic State Management: Kingdom resources and statistics directly influence player combat power during specific game cycles.

- Turn-Based Combat System: A fully functional turn-by-turn battle logic utilizing custom health modifiers, attack patterns, and event triggers.

- Dynamic Gear & Shop System: Players can trade kingdom gold for equipment (Defense, Armor Penetration, and Critical Rate modifiers) before battles. Gear prices and stat values scale dynamically with the game's cycle, requiring strategic economic planning.

- CLI-Based Interface: Operates completely within the terminal.

- Resource Economy: Players must balance the internal state of their kingdom alongside external combat threats.

## Technologies Used

Language: C++ (Standard Library)

Architecture: Object-Oriented Programming (OOP) / Procedural Logic

Environment: Terminal / Command Prompt

## How to Run (Compilation Instructions)

Since this is a native C++ application, you can compile and run it using any standard C++ compiler (like g++, clang, or MSVC).

1. Clone the repository:
   
```bash
git clone https://github.com/TravelMelody/Kingdom-Terminal-RPG.git
cd Kingdom-Terminal-RPG
```



2. Compile the code:
```bash
g++ KingdomTerminalRPG.cpp -o kingdom_rpg
```

3. Run the game:

Windows:
```bash
kingdom_rpg.exe
```


Linux / macOS:
```bash
./kingdom_rpg
```
