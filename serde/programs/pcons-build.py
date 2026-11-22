from pcons import context, write_file

from reflex_build.config import build_dir, build_testing

project = context.current_project
env = project.default_environment

convert = project.Program("reflex-serde-convert", env, sources=["convert.cpp"])
convert.private.link_libs.extend(
    project.get_targets(
        "reflex.serde",
        "reflex.serde.json",
        "reflex.serde.bson",
        "reflex.serde.yaml",
        "reflex.serde.toml",
        "reflex.cli",
    )
)

if build_testing:
    work = build_dir / "serde/programs/convert-tests"
    document = '{"a": 1, "b": "x"}\n'
    named = write_file(work / "in.json", document)
    unnamed = write_file(work / "in", document)

    for fmt in ("json", "yaml", "toml", "bson"):
        project.Test(
            f"serde.convert-inferred-{fmt}",
            convert,
            args=[str(named), str(work / f"inferred.{fmt}")],
            labels=["serde"],
        )
        # No extension on either path, so a format can only come from the flag.
        project.Test(
            f"serde.convert-forced-{fmt}",
            convert,
            args=["-if", "json", "-of", fmt, str(unnamed), str(work / f"forced-{fmt}")],
            labels=["serde"],
        )

    extended = {
        "json": r'{"arr":[1,"x",[false]],"bin":{"$binary":{"base64":"3q0=","subType":"00"}},'
        r'"bin4":{"$binary":{"base64":"3q0=","subType":"04"}},'
        r'"bin8":{"$binary":{"base64":"3q0=","subType":"80"}},"bool":true,'
        r'"cws":{"$code":"f(x)","$scope":{"x":7}},"d128":{"$numberDecimal":"1.5"},'
        r'"dbp":{"$dbPointer":{"$ref":"db.coll","$id":{"$oid":"5061728394a5b6c7d8e9fa0b"}}},'
        r'"doc":{"$set":1},"dt":{"$date":{"$numberLong":"1700000000123"}},"i32":-7,'
        r'"i64":5000000000,"js":{"$code":"g()"},"max":{"$maxKey":1},"min":{"$minKey":1},'
        r'"num":2.5,"oid":{"$oid":"5061728394a5b6c7d8e9fa0b"},'
        r'"re":{"$regularExpression":{"pattern":"^a\"b\\d","options":"im"}},"str":"s",'
        r'"sym":{"$symbol":"s"},"ts":{"$timestamp":{"t":1700000000,"i":2}},'
        r'"u":{"$undefined":true}}',
        "yaml": r"""arr:
  - 1
  - x
  - - false
bin:
  $binary:
    base64: 3q0=
    subType: '00'
bin4:
  $binary:
    base64: 3q0=
    subType: '04'
bin8:
  $binary:
    base64: 3q0=
    subType: '80'
bool: true
cws:
  $code: f(x)
  $scope:
    x: 7
d128:
  $numberDecimal: '1.5'
dbp:
  $dbPointer:
    $ref: db.coll
    $id:
      $oid: 5061728394a5b6c7d8e9fa0b
doc:
  $set: 1
dt:
  $date:
    $numberLong: '1700000000123'
i32: -7
i64: 5000000000
js:
  $code: g()
max:
  $maxKey: 1
min:
  $minKey: 1
num: 2.5
oid:
  $oid: 5061728394a5b6c7d8e9fa0b
re:
  $regularExpression:
    pattern: ^a"b\d
    options: im
str: s
sym:
  $symbol: s
ts:
  $timestamp:
    t: 1700000000
    i: 2
u:
  $undefined: true""",
        "toml": r"""arr = [1, "x", [false]]
bin = { "$binary" = { base64 = "3q0=", subType = "00" } }
bin4 = { "$binary" = { base64 = "3q0=", subType = "04" } }
bin8 = { "$binary" = { base64 = "3q0=", subType = "80" } }
bool = true
cws = { "$code" = "f(x)", "$scope" = { x = 7 } }
d128 = { "$numberDecimal" = "1.5" }
dbp = { "$dbPointer" = { "$ref" = "db.coll", "$id" = { "$oid" = "5061728394a5b6c7d8e9fa0b" } } }
dt = { "$date" = { "$numberLong" = "1700000000123" } }
i32 = -7
i64 = 5000000000
js = { "$code" = "g()" }
max = { "$maxKey" = 1 }
min = { "$minKey" = 1 }
num = 2.5
oid = { "$oid" = "5061728394a5b6c7d8e9fa0b" }
re = { "$regularExpression" = { pattern = '^a"b\d', options = "im" } }
str = "s"
sym = { "$symbol" = "s" }
ts = { "$timestamp" = { t = 1700000000, i = 2 } }
u = { "$undefined" = true }
[doc]
"$set" = 1""",
    }
    fixtures = {
        fmt: write_file(work / f"ext.{fmt}", text) for fmt, text in extended.items()
    }
    round_trip = (
        'set -e; "$1" "$2" "$4" >/dev/null; "$1" "$3" "$5" >/dev/null; cmp "$4" "$5"; '
        '"$1" "$5" "$6" >/dev/null; cmp "$3" "$6"'
    )
    for fmt, fixture in fixtures.items():
        project.Test(
            f"serde.convert-ext-{fmt}",
            "/bin/sh",
            args=[
                "-c",
                round_trip,
                "sh",
                convert,
                str(fixtures["json"]),
                str(fixture),
                str(work / f"ext-{fmt}-ref.bson"),
                str(work / f"ext-{fmt}.bson"),
                str(work / f"ext-{fmt}-back.{fmt}"),
            ],
            labels=["serde"],
        )
