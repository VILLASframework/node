{
  stdenv,
  fetchurl,
  protobuf,
  grpc
}:

stdenv.mkDerivation {
  name = "grpc-server-reflection";

  dontUnpack = true;
  nativeBuildInputs = [ protobuf grpc ];

  src = fetchurl {
    url = "https://raw.githubusercontent.com/grpc/grpc/refs/heads/master/src/proto/grpc/reflection/v1alpha/reflection.proto";
    sha256 = "sha256-c1kQAgFlRRDNFZy7XEaKi+wUtUmXPG+P967poSN67Ec=";
  };

  buildPhase = ''
    # Create a temporary build directory
    mkdir -p tmp
    ln -s $src tmp/reflection.proto

    # Generate code into build_tmp
    protoc --cpp_out=tmp \
           --grpc_out=tmp \
           --plugin=protoc-gen-grpc=${grpc}/bin/grpc_cpp_plugin \
           -I tmp \
           tmp/reflection.proto
  '';

  # Install into standard 'include' path so CMake finds it
  installPhase = ''
    mkdir -p $out/include/grpc/reflection/v1alpha
    cp tmp/*.pb.h $out/include/grpc/reflection/v1alpha/
    cp tmp/*.pb.cc $out/include/grpc/reflection/v1alpha/
  '';
}
