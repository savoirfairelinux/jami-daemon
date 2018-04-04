/**
 * Copyright (c) 2018, Sébastien Blin <sebastien.blin@enconn.fr>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 * * Neither the name of the University of California, Berkeley nor the
 *  names of its contributors may be used to endorse or promote products
 *  derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

extern crate cpython;
extern crate dbus;
extern crate env_logger;
extern crate hyper_native_tls;
extern crate iron;
#[macro_use]
extern crate log;
extern crate regex;
extern crate router;
extern crate rusqlite;
extern crate serde;
extern crate serde_json;
#[macro_use]
extern crate serde_derive;
extern crate time;

pub mod rori;

use rori::manager::Manager;
use rori::api::API;
use serde_json::{Value, from_str};
use std::io::prelude::*;
use std::io::{stdin,stdout,Write};
use std::fs::File;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;
use std::thread;

/**
 * ConfigFile structure
 * TBD
 */
#[derive(Serialize, Deserialize)]
pub struct ConfigFile {
    ring_id: String,
    api_listener: String,
    cert_path: String,
    cert_pass: String,
}

fn clean_string(string: String) -> String {
    let mut s = string.clone();
    if let Some('\n') = s.chars().next_back() {
        s.pop();
    }
    if let Some('\r') = s.chars().next_back() {
        s.pop();
    }
    s
}

/**
 * Generate a config file
 * NOTE: will move into the UI ASAP
 */
fn create_config_file() {
    println!("Config file not found. Please answer to following questions:");
    let mut s = String::new();
    print!("Api endpoint (default: 0.0.0.0:1412): ");
    let _ = stdout().flush();
    stdin().read_line(&mut s).expect("Did not enter a correct string");
    s = clean_string(s);
    let mut endpoint = String::from("0.0.0.0:1412");
    if s.len() > 0 {
        endpoint = s.clone();
    }
    let mut s = String::new();
    print!("Cert path: ");
    let _ = stdout().flush();
    stdin().read_line(&mut s).expect("Did not enter a correct string");
    s = clean_string(s);
    let cert_path = s.clone();
    let mut s = String::new();
    print!("Cert password: ");
    let _ = stdout().flush();
    stdin().read_line(&mut s).expect("Did not enter a correct string");
    s = clean_string(s);
    let cert_pass = s.clone();

    println!("Create an account? y/N: ");
    let _ = stdout().flush();
    let mut s = String::new();
    stdin().read_line(&mut s).expect("Did not enter a correct string");
    s = clean_string(s);
    if s.len() == 0 {
        s = String::from("N");
    }
    if s.to_lowercase() == "y" {
        let mut s = String::new();
        println!("Import archive? y/N: ");
        let _ = stdout().flush();
        stdin().read_line(&mut s).expect("Did not enter a correct string");
        s = clean_string(s);
        if s.len() == 0 {
            s = String::from("N");
        }
        let from_archive = s.to_lowercase() == "y";
        if from_archive {
            println!("path: ");
        } else {
            println!("alias: ");
        }
        let _ = stdout().flush();
        let mut s = String::new();
        stdin().read_line(&mut s).expect("Did not enter a correct string");
        s = clean_string(s);
        let main_info = s.clone();
        println!("password (optional):");
        let _ = stdout().flush();
        let mut s = String::new();
        stdin().read_line(&mut s).expect("Did not enter a correct string");
        s = clean_string(s);
        let password = s.clone();
        Manager::add_account(&*main_info, &*password, from_archive);
        // Let some time for the daemon
        let three_secs = Duration::from_millis(3000);
        thread::sleep(three_secs);
    }

    let accounts = Manager::get_account_list();
    let mut idx = 0;
    println!("Choose an account:");
    for account in &accounts {
        println!("{}. {}", idx, account);
        idx += 1;
    }
    println!("Your choice:");
    let _ = stdout().flush();
    let mut s = String::new();
    stdin().read_line(&mut s).expect("Did not enter a correct string");
    s = clean_string(s);
    if s.len() == 0 {
        s = String::from("0");
    }
    let s = s.parse::<usize>().unwrap_or(0);
    if s >= accounts.len() {
        return;
    }
    let account = &accounts.get(s).unwrap().id;
    let config = ConfigFile {
        ring_id: account.clone(),
        api_listener: endpoint,
        cert_path: cert_path,
        cert_pass: cert_pass
    };
    let config = serde_json::to_string_pretty(&config).unwrap_or(String::new());
    let mut file = File::create("config.json").ok().expect("config.json found.");
    let _ = file.write_all(config.as_bytes());

}

fn main() {
    // Init logging
    env_logger::init();

    // if not config, create it
    if !Path::new("config.json").exists() {
        create_config_file();
    }

    if !Path::new("config.json").exists() {
        error!("No config file found");
        return;
    }

    // This script load config from config.json
    let mut file = File::open("config.json").ok()
        .expect("Config file not found");
    let mut config = String::new();
    file.read_to_string(&mut config).ok()
        .expect("failed to read!");
    let config: Value = from_str(&*config).ok()
                        .expect("Incorrect config file. Please check config.json");

    let shared_manager : Arc<Mutex<Manager>> = Arc::new(Mutex::new(
        Manager::init(config["ring_id"].as_str().unwrap_or(""))
        .ok().expect("Can't initialize ConfigurationManager"))
    );
    let shared_manager_cloned = shared_manager.clone();
    let stop = Arc::new(AtomicBool::new(false));
    let stop_cloned = stop.clone();
    let test = thread::spawn(move || {
        Manager::handle_signals(shared_manager_cloned, stop_cloned);
    });
    let mut api = API::new(shared_manager,
                           String::from(config["api_listener"].as_str().unwrap_or("")),
                           String::from(config["cert_path"].as_str().unwrap_or("")),
                           String::from(config["cert_pass"].as_str().unwrap_or(""))
                       );
    api.start();
    stop.store(false, Ordering::SeqCst);
    let _ = test.join();
    // TODO ncurses for UI and quit
}
