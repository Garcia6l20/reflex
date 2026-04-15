#include <reflex/cli.hpp>

namespace reflex::cli::detail
{
void emit_zsh_source(std::string_view program)
{
  std::println(
      R"zsh(#compdef {0}

_{1}_completion() {{
    local completed=0
    local -a completions
    local -a completions_with_descriptions
    local -a response
    local type value descr
    local i

    (( ! $+commands[{0}] )) && return 1

    response=("${{(@f)$(env _REFLEX_COMP_LINE="${{words[*]}}" _REFLEX_COMP_POINT=$((CURRENT-1)) _REFLEX_COMPLETE=zsh_complete {0})}}")

    if (( ${{#response[@]}} > 0 )); then
        completed=${{response[1]}}
    fi

    i=2
    while (( i + 2 <= ${{#response[@]}} )); do
        type=${{response[i]}}
        value=${{response[i+1]}}
        descr=${{response[i+2]}}

        if [[ "$type" == "plain" ]]; then
            if [[ "$descr" == "_" ]]; then
                completions+=("$value")
            else
                completions_with_descriptions+=("$value":"$descr")
            fi
        elif [[ "$type" == "dir" ]]; then
            _path_files -/
        elif [[ "$type" == "file" ]]; then
            if [[ "$value" == "*" ]]; then
                _path_files
            else
                _path_files -g "$value" -/
            fi
        fi
        (( i += 3 ))
    done

    if [ -n "$completions_with_descriptions" ]; then
        if [[ "$completed" == "1" ]]; then
            _describe -V unsorted completions_with_descriptions -U -S ' '
        else
            _describe -V unsorted completions_with_descriptions -U -S ''
        fi
    fi

    if [ -n "$completions" ]; then
        if [[ "$completed" == "1" ]]; then
            compadd -U -V unsorted -S ' ' -a completions
        else
            compadd -U -V unsorted -S '' -a completions
        fi
    fi
}}

if [[ $zsh_eval_context[-1] == loadautofunc ]]; then
    # autoload from fpath, call function directly
    _{1}_completion "$@"
else
    # eval/source/. command, register function for later
    compdef _{1}_completion {0}
fi
)zsh",
      program, [&] {
        std::string id{program};
        std::ranges::replace(id, '-', '_');
        return id;
      }());
}

void emit_bash_source(std::string_view program)
{
  std::println(
      R"bash(_{1}_completion() {{
    local type value descr
    local completed=0
    local response
    local -a lines
    local i

    response=$(env _REFLEX_COMP_LINE="${{COMP_WORDS[*]}}" _REFLEX_COMP_POINT=$COMP_CWORD _REFLEX_COMPLETE=bash_complete $1)
    mapfile -t lines <<< "$response"

    if [[ ${{#lines[@]}} -gt 0 ]]; then
        completed=${{lines[0]}}
    fi

    if [[ "$completed" == "1" ]]; then
        compopt +o nospace
    else
        compopt -o nospace
    fi

    for (( i=1; i+2<${{#lines[@]}}; i+=3 )); do
        type=${{lines[i]}}
        value=${{lines[i+1]}}
        descr=${{lines[i+2]}}

        case "$type" in
            plain)
                COMPREPLY+=("$value")
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
    complete -o nosort -F _{1}_completion {0}
  }}

_{1}_completion_setup;
)bash",
      program, [&] {
        std::string id{program};
        std::ranges::replace(id, '-', '_');
        return id;
      }());
}
} // namespace reflex::cli::detail