# jamictrl tools

Here are some simple Python tools that can be used from the CLI, without any GUI and without installing any Jami front end. 
These CLI tools can be useful on head-less servers like Raspberry Pis or for managing a Jami installation from the command line, or via SSH.

# jamictrl.py

Perform simple operations on a swarm from the CLI. 

+ --get-all-accounts
+ --get-registered-accounts
+ --get-enabled-accounts
+ --get-all-accounts-details
+ --get-account-details
+ --enable account
+ --disable account
+ and many more

# sendfile.py

Simple Python script to send a file to a swarm from the CLI.

# sendmsg.py

Simple Python script to send a text message to a swarm from the CLI.

# swarm.py

Perform simple operations on a swarm from the CLI. 
Includes operations like 

+ Create a swarm conversation
+ List swarm conversations
+ List conversation members
+ Add Member
+ List requests
+ Accept request
+ Decline request
+ Send text message to swarm
+ Remove conversation
