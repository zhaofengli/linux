with import <nixpkgs> {};

mkShell {
  buildInputs = [
    openssl
    elfutils
    gnumake
    ncurses
    flex
    yacc
  ];
}
