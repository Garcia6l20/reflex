if test -n "{3}" -a -d "{3}"
    if not contains -- "{3}" $fish_user_paths
        set -gx PATH "{3}" $PATH
    end
end

function _{1}_completion
    set -l comp_line (commandline -cp)
    set -l comp_words (string split -n ' ' -- $comp_line)
    set -l comp_point 0
    if string match -rq ' $' -- $comp_line
        set comp_point (count $comp_words)
    else if test (count $comp_words) -gt 0
        set comp_point (math (count $comp_words) - 1)
    end

    set -l response (string split \n -- (env \
        _REFLEX_COMP_LINE=$comp_line \
        _REFLEX_COMP_POINT=$comp_point \
        _REFLEX_COMPLETE=fish_complete "{2}" 2>/dev/null))

    test (count $response) -ge 1; or return

    set -l i 2
    while test $i -le (count $response)
        set -l type $response[$i]
        set -l value $response[(math $i + 1)]
        set -l descr $response[(math $i + 2)]
        set i (math $i + 3)

        if test "$type" = dir
            __fish_complete_directories $value
        else if test "$type" = file
            if test "$value" = "*"
                # Wildcard: complete all paths (files + directories)
                __fish_complete_path
            else
                # Pattern: filter path completions to matching files and directories
                set -l cur (commandline -ct)
                for completion in (__fish_complete_path $cur)
                    set -l name (string split \t -- $completion)[1]
                    if string match -qr '/$' -- $name
                        echo $completion
                    else if string match -q -- $value (string replace -r '^.*/' '' -- $name)
                        echo $completion
                    end
                end
            end
        else if test "$type" = plain
            if test -n "$descr"
                printf '%s\t%s\n' $value $descr
            else
                echo $value
            end
        end
    end
end

complete --no-files --command "{0}" --arguments "(_{1}_completion)"
