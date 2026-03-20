#include <reflex/cli.hpp>

namespace reflex::cli::detail
{
void emit_zsh_source(std::string_view program)
{
  std::println(
      R"zsh(#compdef {0}

_{1}_completion() {{
    local -a completions
    local -a completions_with_descriptions
    local -a response

    (( ! $+commands[{0}] )) && return 1

    response=("${{(@f)$(env _REFLEX_COMP_LINE="${{words[*]}}" _REFLEX_COMP_POINT=$((CURRENT-1)) _REFLEX_COMPLETE=zsh_complete {0})}}")

    for type key descr in ${{response}}; do
        if [[ "$type" == "plain" ]]; then
            if [[ "$descr" == "_" ]]; then
                completions+=("$key")
            else
                completions_with_descriptions+=("$key":"$descr")
            fi
        elif [[ "$type" == "dir" ]]; then
            _path_files -/
        elif [[ "$type" == "file" ]]; then
            if [[ "$key" == "*" ]]; then
                _path_files -f
            else
                _path_files -g "$key"
            fi
        fi
    done

    if [ -n "$completions_with_descriptions" ]; then
        _describe -V unsorted completions_with_descriptions -U
    fi

    if [ -n "$completions" ]; then
        compadd -U -V unsorted -a completions
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

    response=$(env _REFLEX_COMP_LINE="${{COMP_WORDS[*]}}" _REFLEX_COMP_POINT=$COMP_CWORD _REFLEX_COMPLETE=bash_complete $1)

    while IFS= read -r type && IFS= read -r value && IFS= read -r descr; do
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
                if [[ "$value" == "*" ]]; then
                    compopt -o default
                else
                    COMPREPLY=( $(compgen -G "${{COMP_WORDS[COMP_CWORD]}}$value" 2>/dev/null) )
                    [[ ${{#COMPREPLY[@]}} -eq 0 ]] && compopt -o default
                fi
                ;;
        esac
    done <<< "$response"

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