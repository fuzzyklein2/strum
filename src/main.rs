/*******************************************************************************
* 
*   🌎  MAIN
* 
*******************************************************************************/
#![allow(warnings)]
use std::io::{ Error };
use std::sync::{ OnceLock };

use hw::{ ARGUMENTS, CONFIGURATION, FILE_SYSTEM, INPUT };
use hw::config::{ configure, JSON };
use hw::constants::{ BASE_DIR };
use hw::files::{ cwd, FileSystem, home };
use hw::getargs::{ Args };
use hw::logging::{ debug, info, init_log };
use hw::utilities::program_name;

/******************************************************************************
*
*   main()
*
******************************************************************************/

fn main() -> Result<(), Error> {
    configure().expect("Configuration failed!");
    init_log().expect("Log initialization failed!");

    debug(&format!("Program name: {}", program_name()
        .expect("Can't get program name!")));
    let s = cwd().unwrap();
    debug(&format!("Current working directory: {}", s.display()));
    let s = BASE_DIR.display();
    debug(&format!("Base directory: {s}"));
    let home = home().unwrap();
    debug(&format!("User home directory: {}", home.display()));
    debug(&format!("Arguments: {:#?}", ARGUMENTS.get().unwrap().args));


    debug(&format!(r#"File System:
Configuration file: {}
Log file:           {}
"#, FILE_SYSTEM.get().unwrap().config_file.display(),
    FILE_SYSTEM.get().unwrap().log_file.display()
));

    debug(&format!(r#"Configuration:
{}
"#, json::stringify_pretty(CONFIGURATION.get().unwrap().value.clone(), 4)
));
    
    info(&format!("Hello, 🌎 !"));

    // Add any other processing here.

    // peroxide::run()?;
    
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_program_name() {
        let PROGRAM = program_name().unwrap();
        assert!(PROGRAM.starts_with("strum"));
    }
}
