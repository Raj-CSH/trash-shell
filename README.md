# Trash

Trash stands for *trash shell*. This is a simple shell, but it's kinda buggy. This was just a simple learning project, so ¯\\\_(ツ)\_/¯. Use at your own risk!

## Features

 - Unlimited pipes
 - IO (only `stdin` and `stdout` as of now) redirection (buggy when used with pipes)
 - Shell variable expansion
 - Tilde expansion
 - History
 - Filename tab autocompletion
 
## Shell Builtins

 - `cd`
 - `echo`
 - `export`

## TODO

 - `stderr` IO redirection
 - Fix IO redirection when used with pipes
 - Proper signal handling
 - `alias` shell builtin among other shell builtins
 - Persistent shell history
 - Shell configuration file
 - Command tab autocompletion

## Dependencies

- [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html) (installed already on most GNU/Linux distributions and FreeBSD)

## Author

- **Rajarshi Mandal** - [Raj-CSH](https://github.com/Raj-CSH)

## License

This project is licensed under the BSD 2 Clause License - see [LICENSE](https://raw.githubusercontent.com/Raj-CSH/trash-shell/master/LICENSE) file for details.
