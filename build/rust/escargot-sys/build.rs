// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

use std::{env, path::PathBuf, process::Command};

fn run(command: &mut Command) {
    let status = command.status().expect("could not run CMake");
    assert!(status.success(), "{command:?} failed with {status}");
}

fn main() {
    println!("cargo:rerun-if-env-changed=ESCARGOT_LIB_DIR");
    if let Some(directory) = env::var_os("ESCARGOT_LIB_DIR") {
        let directory = PathBuf::from(directory)
            .canonicalize()
            .expect("ESCARGOT_LIB_DIR must name an existing directory");
        println!("cargo:rustc-link-search=native={}", directory.display());
        println!("cargo:rustc-link-lib=dylib=escargot");
        println!("cargo:rerun-if-changed={}", directory.display());
        return;
    }

    assert_eq!(env::var("HOST"), env::var("TARGET"),
        "For cross compilation, build a shared C API library for the target and set ESCARGOT_LIB_DIR");
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    assert!(os == "linux" || os == "macos",
        "Bundled builds support Linux and macOS; otherwise set ESCARGOT_LIB_DIR to a shared C API library");
    let root = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap())
        .join("../../..")
        .canonicalize()
        .unwrap();
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("native");
    for path in [
        "CMakeLists.txt",
        "RELEASE_VERSION",
        "src",
        "build/config.cmake",
        "build/target.cmake",
        "build/escargot.cmake",
        "third_party",
    ] {
        println!("cargo:rerun-if-changed={}", root.join(path).display());
    }
    for name in [
        "CC",
        "CXX",
        "CFLAGS",
        "CXXFLAGS",
        "LDFLAGS",
        "CMAKE_GENERATOR",
    ] {
        println!("cargo:rerun-if-env-changed={name}");
    }
    run(Command::new("cmake")
        .arg("-S")
        .arg(&root)
        .arg("-B")
        .arg(&output)
        .args([
            "-DCMAKE_BUILD_TYPE=Release",
            "-DESCARGOT_C_API=ON",
            "-DESCARGOT_BUILD_SHARED_LIBS=OFF",
            "-DESCARGOT_BUILD_GC_SHARED_LIBS=OFF",
            "-DESCARGOT_ENABLE_SHELL=OFF",
            "-DESCARGOT_BUILD_CCTEST=OFF",
            "-DESCARGOT_NAPI=OFF",
            "-DESCARGOT_LIBICU_SUPPORT=OFF",
            "-DESCARGOT_LIBICU_SUPPORT_VENDORED=OFF",
            "-DESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN=OFF",
            "-DESCARGOT_TEMPORAL=OFF",
            "-DESCARGOT_WASM=OFF",
            "-DESCARGOT_THREADING=ON",
        ]));
    run(Command::new("cmake")
        .arg("--build")
        .arg(&output)
        .args(["--target", "escargot", "--config", "Release", "--parallel"])
        .arg(env::var("NUM_JOBS").unwrap_or_else(|_| "1".into())));
    for directory in [
        output.clone(),
        output.join("Release"),
        output.join("third_party/GCutil"),
        output.join("third_party/GCutil/Release"),
    ] {
        println!("cargo:rustc-link-search=native={}", directory.display());
    }
    for library in ["escargot", "gc-lib", "libsimdutf", "libbf"] {
        println!("cargo:rustc-link-lib=static={library}");
    }
    println!(
        "cargo:rustc-link-lib={}",
        if os == "macos" { "c++" } else { "stdc++" }
    );
    for library in ["pthread", "m", "dl"] {
        println!("cargo:rustc-link-lib={library}");
    }
    if os == "linux" {
        println!("cargo:rustc-link-lib=atomic");
    }
}
