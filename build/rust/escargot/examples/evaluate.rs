// Copyright (c) 2026-present Samsung Electronics Co., Ltd
// SPDX-License-Identifier: LGPL-2.1-or-later

use escargot::{Error, Runtime};

fn main() -> Result<(), Error> {
    let runtime = Runtime::new()?;
    let context = runtime.vm()?.context()?;
    println!("{}", context.eval("21 * 2", "example.js")?.as_number()?);
    if let Err(Error::JavaScript(exception)) = context.eval("throw 'hello from JS'", "throw.js") {
        eprintln!("Caught: {}", exception.value.to_utf8()?);
        for frame in exception.stack {
            eprintln!("  {:?}:{}:{}", frame.source_name, frame.line, frame.column);
        }
    }
    Ok(())
}
