# Arena Assault Wii U — ALPHA 0.8.3

Gotowa wersja **Arena Assault** dla **Nintendo Wii U** uruchamiana przez **Aroma**.

To repozytorium jest przeznaczone wyłącznie do dystrybucji gotowego pliku **WUHB**. Kod źródłowy i narzędzia do budowania nie są tutaj publikowane.

## Instalacja

Skopiuj zawartość repozytorium/paczki na kartę SD tak, aby plik gry znajdował się dokładnie tutaj:

```text
SD:/wiiu/apps/ArenaAssault/ArenaAssault.wuhb
```

Najprościej:

1. Wyłącz Wii U i wyjmij kartę SD.
2. Skopiuj katalog `wiiu` do katalogu głównego karty SD.
3. Włóż kartę SD do Wii U.
4. Uruchom środowisko Aroma.
5. Arena Assault powinna pojawić się w menu Wii U.

Nie trzeba osobno kopiować modeli, tekstur ani `glslcompiler.rpl`.

## Sterowanie menu

- **A** — rozpocznij / wznów,
- **PLUS** — pauza / wznów,
- **B podczas pauzy** — powrót do menu.

## Plik wydania

```text
wiiu/apps/ArenaAssault/ArenaAssault.wuhb
```

Rozmiar:

```text
1 537 124 bajty
```

SHA-256:

```text
fe634edf6c5520977dbb0e6db1d115bc14d3bc3448cc7b27325ca3b322a4101b
```

Suma kontrolna pozwala sprawdzić, czy pobrany plik WUHB jest identyczny z wydaniem **ALPHA 0.8.3**.

## Dokumentacja

- `ARENA_ASSAULT_INSTALL.txt` — krótka instrukcja instalacji,
- `CHANGELOG.md` — historia publicznych wydań tego repozytorium,
- `docs/RELEASE_0.8.3.md` — informacje techniczne o bieżącym wydaniu.

## Zakres repozytorium

Repozytorium zawiera gotowy plik wykonywalny WUHB i dokumentację. Nie zawiera kodu źródłowego, plików projektu CMake, skryptów budowania ani innych elementów developerskich.

## Licencja

Warunki licencji znajdują się w pliku `LICENSE`.
