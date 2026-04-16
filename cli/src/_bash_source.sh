
if [[ -n "{3}" && -d "{3}" ]]; then
    case ":$PATH:" in
        *":{3}:"*) ;;
        *) export PATH="{3}:$PATH" ;;
    esac
fi

_{1}_completion() {{
    local type value descr
    local completed=0
    local response
    local -a lines
    local current_raw=""
    local current_bash="${{COMP_WORDS[COMP_CWORD]}}"
    local i

    # Use raw line up to cursor
    local reflex_line="${{COMP_LINE:0:COMP_POINT}}"

    # Compute word index compatible with space-token parser
    local -a reflex_words=()
    read -r -a reflex_words <<< "$reflex_line"
    local reflex_point=0
    if [[ "$reflex_line" =~ [[:space:]]$ ]]; then
    reflex_point=${{#reflex_words[@]}}
    else
        if (( ${{#reflex_words[@]}} > 0 )); then
            reflex_point=$(( ${{#reflex_words[@]}} - 1 ))
        fi
    fi

    response=$(env \
        _REFLEX_COMP_LINE="$reflex_line" \
        _REFLEX_COMP_POINT=$reflex_point \
        _REFLEX_COMPLETE=bash_complete "{2}"{4})
    mapfile -t lines <<< "$response"

    if [[ ${{#lines[@]}} -gt 0 ]]; then
        completed=${{lines[0]}}
    fi

    if [[ "$completed" == "1" ]]; then
        compopt +o nospace
    else
        compopt -o nospace
    fi

    if (( reflex_point >= 0 && reflex_point < ${{#reflex_words[@]}} )); then
        current_raw="${{reflex_words[reflex_point]}}"
    fi

    for (( i=1; i+2<${{#lines[@]}}; i+=3 )); do
        type=${{lines[i]}}
        value=${{lines[i+1]}}
        descr=${{lines[i+2]}}

        case "$type" in
            plain)
                # Bash may split assignment operators from the current word.
                # In that case, completion should insert only the RHS fragment.
                if [[ "$current_raw" == *"="* && "$value" == *"="* ]]; then
                    local lhs_with_op="${{current_raw%=*}}="
                    if [[ "$value" == "$lhs_with_op"* ]]; then
                        COMPREPLY+=("${{value#"$lhs_with_op"}}")
                    else
                        COMPREPLY+=("${{value#*=}}")
                    fi
                else
                    COMPREPLY+=("$value")
                fi
                ;;
            dir)
                COMPREPLY=()
                compopt -o dirnames
                ;;
            file)
                COMPREPLY=()
                compopt -o filenames
                if [[ "$value" == "*" ]]; then
                    compopt -o default
                else
                    COMPREPLY=( $(compgen -G "${{COMP_WORDS[COMP_CWORD]}}$value" 2>/dev/null) $(compgen -d -- "${{COMP_WORDS[COMP_CWORD]}}" 2>/dev/null) )
                    [[ ${{#COMPREPLY[@]}} -eq 0 ]] && compopt -o default
                fi
                ;;
        esac
    done

    return 0
}}

_{1}_completion_setup() {{
  complete -o nosort -F _{1}_completion "{0}"
}}

_{1}_completion_setup;
