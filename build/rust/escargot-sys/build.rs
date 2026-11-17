// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

use std::{env, fs, path::PathBuf, process::Command};

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
        "build/VendoredICU.cmake",
        "build/icu-filters",
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
    let icu = env::var_os("CARGO_FEATURE_ICU").is_some();
    let temporal = env::var_os("CARGO_FEATURE_TEMPORAL").is_some();
    let bundled = env::var_os("CARGO_FEATURE_ICU_BUNDLED").is_some();
    let mut configure = Command::new("cmake");
    configure
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
            // Restore platform defaults when reconfiguring a prior cache.
            "-UESCARGOT_LIBICU_SUPPORT_VENDORED",
            "-UESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN",
            "-DESCARGOT_WASM=OFF",
            "-DESCARGOT_THREADING=ON",
        ])
        .arg(format!(
            "-DESCARGOT_LIBICU_SUPPORT={}",
            if icu { "ON" } else { "OFF" }
        ))
        .arg(format!(
            "-DESCARGOT_TEMPORAL={}",
            if temporal { "ON" } else { "OFF" }
        ))
        .arg(format!(
            "-DESCARGOT_VENDORED_ICU_BUILD_DIR={}",
            output.join("vendored-icu").display()
        ));
    if bundled {
        configure.arg("-DESCARGOT_LIBICU_SUPPORT_VENDORED=ON");
    } else if !icu {
        configure.args([
            "-DESCARGOT_LIBICU_SUPPORT_VENDORED=OFF",
            "-DESCARGOT_LIBICU_SUPPORT_WITH_DLOPEN=OFF",
        ]);
    }
    run(&mut configure);
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
    let cache = fs::read_to_string(output.join("CMakeCache.txt")).unwrap();
    let vendored = cache
        .lines()
        .any(|line| line == "ESCARGOT_ICU_MODE:INTERNAL=vendored");
    if icu && vendored {
        println!(
            "cargo:rustc-link-search=native={}",
            output.join("vendored-icu/install/lib").display()
        );
        // rustc bundles these static archives into the sys rlib, retaining ICU
        // data along with the implementation for downstream Rust executables.
        for library in ["icui18n", "icuuc", "icudata"] {
            println!("cargo:rustc-link-lib=static={library}");
        }
    } else if icu {
        for directory in [
            output.join("third_party/runtime_icu_binder"),
            output.join("third_party/runtime_icu_binder/Release"),
        ] {
            println!("cargo:rustc-link-search=native={}", directory.display());
        }
        println!("cargo:rustc-link-lib=static=runtime-icu-binder-static");
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
