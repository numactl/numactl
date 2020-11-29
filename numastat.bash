# bash completion for numastat(8)

_numastat() {

	local cur=${COMP_WORDS[COMP_CWORD]}
	local prev=${COMP_WORDS[COMP_CWORD - 1]}

	local -A options=(
		["-V"]=""
		["-c"]=""
		["-m"]=""
		["-n"]=""
		["-p"]=""
		["-s"]="*"
		["-v"]=""
		["-z"]=""
	)

	local option word
	for word in "${COMP_WORDS[@]}"; do
		[[ $word != "$cur" || $prev != -p ]] || continue
		for option in "${!options[@]}"; do
			# shellcheck disable=SC2254
			case $word in
				$option | $option${options[$option]-})
					unset "options[$option]" "options[-V]"
					[[ $word != -V ]] || options=()
					;;
			esac
		done
	done

	case $prev in
		-V) return 0 ;;
		-p) ;;
		-*)
			# shellcheck disable=SC2207,SC2016
			COMPREPLY=($(compgen -W '${!options[@]}' -- "$cur"))
			return
			;;
	esac

	_pnames 2>/dev/null || : # from bash-completion, if around
	# shellcheck disable=SC2207,SC2016
	COMPREPLY+=($(compgen -W '$(command ps ax -o pid=)' -- "$cur"))
} &&
	complete -F _numastat numastat
