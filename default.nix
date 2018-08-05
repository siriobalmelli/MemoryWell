{ # deps
  system ? builtins.currentSystem,
  nixpkgs ? import <nixpkgs> { inherit system; },
  nonlibc ? nixpkgs.nonlibc or import <nonlibc> { inherit system;
    inherit buildtype;
    inherit compiler;
    inherit dep_type;
    inherit mesonFlags;
  },
  # options
  buildtype ? "release",
  compiler ? "clang",
  dep_type ? "shared",
  mesonFlags ? ""
}:

# note that "nonlibc" above should not be clobbered by this
with import <nixpkgs> { inherit system; };

stdenv.mkDerivation rec {
  name = "memorywell";
  version = "0.1.9";
  meta = with stdenv.lib; {
    description = "nonblocking circular buffer";
    homepage = https://siriobalmelli.github.io/memorywell/;
    license = licenses.lgpl21Plus;
    platforms = platforms.unix;
    maintainers = [ "https://github.com/siriobalmelli" ];
  };

  #env = buildEnv { name = name; paths = nativeBuildInputs; };
  outputs = [ "out" ];

  buildInputs = [
    clang
    meson
    ninja
    nonlibc
    pandoc
    pkgconfig
    dpkg
    fpm
    rpm
    zip
  ];

  # just work with the current directory (aka: Git repo), no fancy tarness
  src = ./.;

  # Override the setupHook in the meson nix derivation,
  # so that meson doesn't automatically get invoked from there.
  meson = pkgs.meson.overrideAttrs ( oldAttrs: rec { setupHook = ""; });

  # don't harden away position-dependent speedups for static builds
  hardeningDisable = [ "pic" "pie" ];

  # build
  mFlags = mesonFlags
    + " --buildtype=${buildtype}"
    + " -Ddep_type=${dep_type}";

  configurePhase = ''
      echo "------------------ build env --------------------------"
      env
      echo "-------------------------------------------------------"
      echo "flags: $mFlags"
      echo "prefix: $out"
      CC=${compiler} meson --prefix=$out build $mFlags
      cd build
  '';

  buildPhase = "ninja";
  doCheck = false;
  installPhase = "ninja install";

  # Build packages outside $out then move them in: fpm seems to ignore
  #+	the '-x' flag that we need to avoid packaging packages inside packages
  fixupPhase = ''
      mkdir temp
      for pk in "deb" "rpm" "tar" "zip"; do
          if ! fpm -f -t $pk -s dir -p temp/ -n $name -v $version \
              --description "${meta.description}" \
              --license "${meta.license.spdxId}" \
              --url "${meta.homepage}" \
              --maintainer "${builtins.head meta.maintainers}" \
              "$out/=/"
          then
              echo "ERROR (non-fatal): could not build $pk package" >&2
          fi
      done
      mkdir -p $out/var/cache/packages
      mv -fv temp/* $out/var/cache/packages/
  '';

  # Allow YouCompleteMe and other tooling to see into the byzantine
  #+	labyrinth of library includes.
  # TODO: this string manipulation ought to be done in Nix.
  shellHook=''export CPATH=$(echo $NIX_CFLAGS_COMPILE | sed "s/ \?-isystem /:/g")'';
}
