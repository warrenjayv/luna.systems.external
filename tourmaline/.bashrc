# .bashrc

# Source global definitions
if [ -f /etc/bashrc ]; then
	. /etc/bashrc
fi

# User specific environment
if ! [[ "$PATH" =~ "$HOME/.local/bin:$HOME/bin:" ]]
then
    PATH="$HOME/.local/bin:$HOME/bin:$PATH"
fi
export PATH

# Uncomment the following line if you don't like systemctl's auto-paging feature:
# export SYSTEMD_PAGER=

# User specific aliases and functions
alias ls='ls --color=auto'
alias vi='vim'

export LUNA="/home/warren/Darkman/lunar.project/luna.pro.warfare"
export CONS="/lib/kbd/consolefonts"

force_color_prompt=yes
if [[ $color_prompt = yes ]]
then
  echo "non"
fi

# default: [\u@\h \W]\$
# magenta: \u001b[35;1m
# format: \033[38;2;r;g;bm
export PS1="\033[94m\$(date +\"%m-%d-%Y %H:%M\")\033[0m\033[4m\$(pwd)\033[0m\n\033[39;40m\033[35;1;40m[\u@\h]\033[31;1;60m<$(tty)>\033[39;40m\$ "

export TERM=xterm-256color

