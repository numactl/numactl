_numactl() {

	local -v curr=${COMP_WORDS[COMP_CWORD]}
	local -v prev=${COMP_WORDS[COMP_CWORD - 1]}

	local -A command_opts=(
		["--all"]=-a
		["--interleave="]=-i
		["--preferred="]=-p
		["--physcpubind="]=-C
		["--cpunodebind="]=-N
		["--membind="]=-m
		["--localalloc="]=-l
	)
	local -A show_opts=(
		["--show"]=-s
	)
	local -A hardware_opts=(
		["--hardware"]=-H
	)
	local -A policy_opts=(  # XXX ='s here
		["--length="]=-l
		["--offset="]=-o
		["--shmmode="]=-M
		["--strict"]=-t
		["--shmid="]=-I
		["--shm="]=-S
		["--file="]=-f
		["--huge"]=-u
		["--touch"]=-T
	)
	local -A policy_actions=(
		["--interleave"]=-i
		["--preferred"]=-p
		["--membind"]=-m
		["--localalloc"]=-l
	)

	local -v i j word mode
        for in ${!COMP_WORDS[@]}; do
            word=${COMP_WORDS[i]}
		for j in ${!show_opts[@]}; do
			case $word in
				$j | ${show_opts[j]})
					mode=show
					break 2
					;;
                                $j*)
                                    if [[ $j == *= ]]; then
					mode=show
					break 2
                                    fi
			esac
		done
		for j in ${!hardware_opts[@]}; do
			case $word in
				$j | ${hardware_opts[j]})
					mode=hardware
					break 2
					;;
			esac
		done
		for j in ${!policy_opts[@]}; do
			case $word in
				$j | ${policy_opts[j]})
					mode=policy
					break 2
					;;
			esac
		done
		for j in ${!command_opts[@]}; do
			case $word in
				$j) # TODO "${command_opts[j]}" + conflict w/policy mode triggers: if followed by non-opt, set command mode, else pass through
					mode=command
					break 2
					;;
			esac
		done
	done

	case ${mode-} in
		show | hardware)
			return 0
			;;
		command)
			case $prev in
				*) ;; # TODO
			esac
			return 0
			;;
		policy)
			case $prev in
				*) ;; # TODO
			esac
			return 0
			;;
		*)
			# TODO offer all _opts
			return 0
			;;
	esac

	# --length | -l : g/m/k suffix

} &&
	complete -F _numactl numactl
