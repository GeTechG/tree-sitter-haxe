// @ts-check
/// <reference types="tree-sitter-cli/dsl" />

const PREC = {
  CONTROL: 0,
  ASSIGN: 1,
  TERNARY: 2,
  NULL_COALESCE: 3,
  LOGICAL_OR: 4,
  LOGICAL_AND: 5,
  BIT_OR: 6,
  BIT_XOR: 7,
  BIT_AND: 8,
  EQUALITY: 10,
  COMPARE: 10,
  IS: 10,
  IN: 10,
  SHIFT: 13,
  ADD: 14,
  MULTIPLY: 15,
  RANGE: 16,
  UNARY: 21,
  POSTFIX: 18,
  PRIMARY: 19,
  CALL: 20,
  // BLOCK: 26,
  OBJECT_DECL: 27,
  TYPE_ANNOTATION: 29,
  TYPE_PARAMS: 31,
  CONDITIONAL: 1001,
  MACRO: -1,
};

/**
 * @param {string|Rule} sep - The separator token or rule
 * @param {Rule} rule - The element rule
 * @returns {Rule}
 */
function sep1(sep, rule) {
  return seq(rule, repeat(seq(sep, rule)));
}
/**
 * @param {string|Rule} sep
 * @param {Rule} rule
 * @returns {Rule}
 */
function sep(sep, rule) {
  return optional(sep1(sep, rule));
}
/** @param {Rule} rule @returns {Rule} */
const commaSep1 = (rule) => sep1(",", rule);
/** @param {Rule} rule @returns {Rule} */
const commaSep = (rule) => sep(",", rule);
/** @param {Rule} rule @returns {Rule} */
const dotSep1 = (rule) => sep1(".", rule);
/** @param {Rule} rule @returns {Rule} */
const dotSep = (rule) => sep(".", rule);

const RESERVED_KEYWORDS = [
  "abstract",
  "as",
  "break",
  "case",
  "cast",
  "catch",
  "class",
  "continue",
  "default",
  "do",
  "dynamic",
  "else",
  "enum",
  "extends",
  "extern",
  "false",
  "final",
  "for",
  "function",
  "if",
  "implements",
  "import",
  "in",
  "inline",
  "interface",
  "macro",
  "new",
  "null",
  "operator",
  "overload",
  "override",
  "package",
  "private",
  "public",
  "return",
  "static",
  "super",
  "switch",
  "this",
  "throw",
  "true",
  "try",
  "typedef",
  "untyped",
  "using",
  "var",
  "while",
];

export default grammar({
  name: "haxe",
  extras: ($) => [/\s+/, $.comment, $.conditional],
  externals: ($) => [$.InlineXml, $._float_trailing_dot],
  reserved: {
    global: (_) => RESERVED_KEYWORDS,
  },
  conflicts: ($) => [
    [
      $.AbstractType,
      $.ClassMethod,
      $.ClassType,
      $.ClassVar,
      $.DefType,
      $.EnumType,
    ],
    [
      $.AbstractType,
      $.ClassMethod,
      $.ClassType,
      $.ClassVar,
      $.DefType,
      $.EnumType,
      $._conditional_body,
    ],
    [$.AbstractType, $.ClassType, $.DefType, $.EnumType],
    [$.AbstractType, $.ClassType, $.ClassVar, $.ClassMethod],
    [
      $.AbstractType,
      $.ClassType,
      $.ClassVar,
      $.ClassMethod,
      $._conditional_body,
    ],
    [$.AbstractType, $.ClassType, $._conditional_body],
    [$.AbstractType, $.ClassType],
    [$.AbstractType, $.DefType, $.EnumType, $._conditional_body],
    [$.AbstractType, $.DefType, $.EnumType],
    [$.ClassMethod, $._conditional_body],
    [$.ClassType, $.ClassMethod],
    [$.ClassType, $.ClassVar, $.ClassMethod, $._conditional_body],
    [$.ClassType, $.ClassVar, $.ClassMethod],
    [$.ClassType, $.ClassVar, $.ClassMethod, $.DefType, $.EnumType],
    [
      $.ClassType,
      $.ClassVar,
      $.ClassMethod,
      $.DefType,
      $.EnumType,
      $._conditional_body,
    ],
    [$.ClassType, $.DefType, $.EnumType, $._conditional_body],
    [$.ClassType, $.DefType, $.EnumType],
    [$.ClassType, $._conditional_body],
    [$.ClassVar, $.ClassMethod, $._conditional_body],
    [$.ClassVar, $.ClassMethod],
    [$.ClassVar, $._conditional_body],
    [$.DefType, $.EnumType, $._conditional_body],
    [$.EBinop],
    [$.EBlock, $.EObjectDecl],
    [$.ECall, $.EFunction, $.ClassMethod],
    [$.ECall],
    [$.EFunction],
    [$.TypePath],
    [$._EConst, $.FunctionArg, $.compile_condition],
    [$.FunctionArg, $.compile_condition],
    [$._EConst, $.FunctionArg],
    [$._EConst, $._expr_lhs, $.compile_condition],
    [$._EConst, $._expr_lhs],
    [$._EConst, $.compile_condition],
    [$._Expr, $._expr_lhs],
    [$._block_or_expr, $._comprehension_body],
    [$._block_or_expr],
    [$._base_type, $.optional],
    [$._class_field, $._conditional_body],
    [$._dot_path],
    [$._expr_atom, $._expr_value],
    [$._expr_lhs, $.compile_condition],
    [$._expr_meta, $.ECall],
    [$._expr_prim, $._expr_value],
    [$._expr_value, $._Expr],
    [$._expr_value, $._expr_lhs],
    [$._type_decl, $._class_field, $._conditional_body],
    [$._type_decl, $._conditional_body],
    [$.cases],
    [$.import, $._dot_path],
    [$.ClassType, $.EnumType],
    [$.FunctionArg, $.wildcard_pattern],
  ],
  inline: ($) => [
    $._semicolon,
    $.visibility,
    $.rest,
    $._expr_postfix,
    $._identifier,
    $._type_name,
  ],
  word: ($) => $.identifier,
  rules: {
    module: ($) =>
      seq(
        optional($.package),
        repeat(choice($.import, $.using)),
        repeat(choice($._type_decl, $._expr_statement)),
      ),

    //////////////////////////////////////////////////////////////////////////

    package: ($) =>
      seq("package", optional(field("path", $._dot_path)), $._semicolon),

    import: ($) =>
      seq(
        "import",
        choice(
          seq(
            optional(field("path", seq($._dot_path, "."))),
            field("module", $.type_name),
            optional(field("sub", repeat1(seq(".", $.identifier)))),
            optional(
              choice(
                seq(".", $.wildcard),
                seq(
                  choice("as", "in"),
                  field("alias", choice($.type_name, $.identifier)),
                ),
              ),
            ),
          ),
          seq(
            field(
              "path",
              seq(
                dotSep1($.package_name),
                choice("as", "in"),
                field("alias", choice($.type_name, $.identifier)),
              ),
            ),
          ),
          seq(field("path", repeat1(seq($.package_name, "."))), $.wildcard),
        ),
        $._semicolon,
      ),

    using: ($) =>
      seq(
        "using",
        optional(field("path", seq($._dot_path, "."))),
        repeat(seq($.type_name, ".")),
        field("type", $.type_name),
        $._semicolon,
      ),

    ///////////////////////////////////////////////////////////////////////////

    _expr_statement: ($) => seq($._Expr, optional($._semicolon)),

    _Expr: ($) => choice($.EBinop, $.ETernary, $.EUnop, $._expr_postfix),
    _expr_postfix: ($) => choice($.ECall, $.EField, $.EArray, $._expr_prim),
    _expr_prim: ($) =>
      choice(
        $._expr_atom,
        $.EBreak,
        $.EContinue,
        $.EFor,
        $.EIf,
        $.EReturn,
        $.ESwitch,
        $.EThrow,
        $.ETry,
        $.EVars,
        $.EWhile,
        $._expr_meta,
        $.EBlock,
      ),
    _expr_atom: ($) =>
      choice(
        $._EConst,
        $._EParenthesis,
        $.EObjectDecl,
        $.EArrayDecl,
        $.ENew,
        $.InlineXml,
      ),
    _expr_meta: ($) =>
      choice(
        $.EArrowFunction,
        $.ECast,
        $.ECheckType,
        $.EFunction,
        $.EMeta,
        $.EUntyped,
        //
        $.macro,
        $.reification,
        $.type_trace,
        $.wildcard_pattern,
      ),
    _expr_value: ($) =>
      choice(
        $._expr_postfix,
        $._expr_atom,
        $.EObjectDecl,
        $.EArrayDecl,
        $.ENew,
      ),
    _expr_lhs: ($) => choice($._identifier, $.EField, $.EArray),

    _block_or_expr: ($) =>
      choice(
        prec(1, $.EBlock),
        prec.dynamic(-1, seq($._Expr, optional($._semicolon))),
      ),

    ECall: ($) =>
      prec.left(
        PREC.CALL,
        seq(
          optional("inline"),
          field(
            "callee",
            choice(
              $._EParenthesis,
              $.ECall,
              $.EArray,
              $.EField,
              $._identifier,
              $.super,
            ),
          ),
          "(",
          field("args", commaSep(choice($.reification, $._Expr))),
          ")",
        ),
      ),
    EField: ($) =>
      prec.left(
        PREC.CALL,
        seq(
          field("object", $._Expr),
          field("op", choice(".", "?.")),
          field("name", choice($._identifier, $._soft_keyword_ident)),
        ),
      ),
    EArray: ($) =>
      prec.left(
        PREC.CALL,
        seq(field("array", $._Expr), "[", field("index", $._Expr), "]"),
      ),
    _EConst: ($) =>
      choice(
        $.Int,
        $.Float,
        $.String,
        $.Regexp,
        $.true,
        $.false,
        $.null,
        $.this,
        $.super,
        $._identifier,
      ),
    _EParenthesis: ($) => prec(PREC.PRIMARY, seq("(", $._Expr, ")")),
    ENew: ($) =>
      prec.left(PREC.CALL, seq("new", $.TypePath, "(", commaSep($._Expr), ")")),
    EFunction: ($) =>
      prec.right(PREC.CONTROL - 1, seq(optional("inline"), $._function_decl)),
    EArrowFunction: ($) =>
      prec.right(
        PREC.CONTROL - 1,
        seq(
          choice(
            seq("(", ")"),
            field("args", $._identifier),
            seq(field("args", $._function_args), optional($._type_annotation)),
          ),
          "->",
          field("body", $._block_or_expr),
        ),
      ),
    EVars: ($) =>
      prec.right(
        PREC.ASSIGN,
        seq(
          choice("var", "final"),
          commaSep1(
            seq(
              field("name", $._identifier),
              optional(field("type", $._type_annotation)),
              optional(
                seq(
                  "=",
                  field("value", choice(prec(1, $.InlineXml), $._Expr)),
                ),
              ),
            ),
          ),
        ),
      ),
    ETernary: ($) =>
      prec.right(
        PREC.TERNARY,
        seq(
          field("cond", $._Expr),
          "?",
          field("if", $._Expr),
          ":",
          field("else", $._Expr),
        ),
      ),
    EBinop: ($) => {
      const BINOPS = [
        {
          ops: [
            "=>",
            "=",
            "+=",
            "-=",
            "*=",
            "/=",
            "%=",
            "&=",
            "|=",
            "^=",
            "<<=",
            ">>=",
            ">>>=",
            "&&=",
            "||=",
            "??=",
          ],
          prec: PREC.ASSIGN,
          assoc: "right",
        },
        { ops: ["??"], prec: PREC.NULL_COALESCE, assoc: "right" },
        { ops: ["||"], prec: PREC.LOGICAL_OR, assoc: "left" },
        { ops: ["&&"], prec: PREC.LOGICAL_AND, assoc: "left" },
        { ops: ["|"], prec: PREC.BIT_OR, assoc: "left" },
        { ops: ["^"], prec: PREC.BIT_XOR, assoc: "left" },
        { ops: ["&"], prec: PREC.BIT_AND, assoc: "left" },
        { ops: ["==", "!="], prec: PREC.EQUALITY, assoc: "non" },
        { ops: [">", ">=", "<", "<="], prec: PREC.COMPARE, assoc: "non" },
        { ops: ["is"], prec: PREC.IS, rhs: $.TypePath, assoc: "non" },
        { ops: ["in"], prec: PREC.IN, assoc: "non" },
        { ops: ["<<", ">>", ">>>"], prec: PREC.SHIFT, assoc: "left" },
        { ops: ["+", "-"], prec: PREC.ADD, assoc: "left" },
        { ops: ["*", "/", "%"], prec: PREC.MULTIPLY, assoc: "left" },
        { ops: ["..."], prec: PREC.RANGE, assoc: "non" },
      ];
      return choice(
        ...BINOPS.map(({ ops, prec: level, assoc, rhs }) => {
          const op = ops.length === 1 ? ops[0] : choice(...ops);
          const rule = seq(
            field("left", $._Expr),
            field("op", op),
            field("right", rhs || $._Expr),
          );
          if (assoc === "right") return prec.right(level, rule);
          if (assoc === "left") return prec.left(level, rule);
          return prec(level, rule);
        }),
      );
    },
    EUnop: ($) =>
      choice(
        prec.right(
          PREC.UNARY,
          seq(
            field("op", choice("++", "--", "+", "-", "!", "~", "...")),
            field("operand", $._expr_value),
          ),
        ),
        prec.left(
          PREC.POSTFIX,
          seq(field("operand", $._expr_lhs), field("op", choice("++", "--"))),
        ),
      ),
    EBlock: ($) => prec.dynamic(-1, seq("{", repeat($._expr_statement), "}")),
    EObjectDecl: ($) =>
      prec.dynamic(
        -1,
        seq(
          "{",
          commaSep(
            seq(
              field("name", choice($.identifier, $.String)),
              ":",
              field("value", $._Expr),
            ),
          ),
          "}",
        ),
      ),
    EArrayDecl: ($) =>
      prec(
        1,
        seq(
          "[",
          optional(
            choice(
              commaSep($._Expr),
              alias($._comprehension_for, $.EFor),
              $.EWhile,
            ),
          ),
          "]",
        ),
      ),
    _comprehension_for: ($) =>
      seq(
        "for",
        "(",
        choice(
          seq(field("key", $.identifier), "=>", field("value", $.identifier)),
          field("var", $.identifier),
        ),
        "in",
        field("iterable", $._Expr),
        ")",
        field("body", $._comprehension_body),
      ),
    _comprehension_if: ($) =>
      seq(
        "if",
        "(",
        field("cond", $._Expr),
        ")",
        field("if", $._comprehension_body),
      ),
    _comprehension_body: ($) =>
      choice(
        alias($._comprehension_for, $.EFor),
        alias($._comprehension_if, $.EIf),
        $._Expr,
      ),
    EReturn: ($) => prec.right(PREC.CONTROL, seq("return", optional($._Expr))),
    EBreak: (_) => prec.right(PREC.CONTROL, "break"),
    EContinue: (_) => prec.right(PREC.CONTROL, "continue"),
    EThrow: ($) =>
      prec.right(PREC.CONTROL, seq("throw", field("expr", $._Expr))),
    ECast: ($) =>
      prec.right(
        PREC.CALL + 1,
        seq(
          "cast",
          choice(
            field("expr", $._Expr),
            seq(
              "(",
              field("expr", $._Expr),
              ",",
              field("type", $.ComplexType),
              ")",
            ),
          ),
        ),
      ),
    EUntyped: ($) => prec.right(seq("untyped", $._Expr)),
    EFor: ($) =>
      prec(
        PREC.PRIMARY + 1,
        seq(
          "for",
          "(",
          choice(
            seq(field("key", $.identifier), "=>", field("value", $.identifier)),
            field("var", $.identifier),
          ),
          "in",
          field("iterable", choice($._Expr, $.EConditional)),
          ")",
          field("body", $._block_or_expr),
        ),
      ),
    EIf: ($) =>
      prec.right(
        PREC.CONTROL + 1,
        seq(
          "if",
          "(",
          field("cond", $._Expr),
          ")",
          field("if", $._block_or_expr),
          optional(seq("else", field("else", $._block_or_expr))),
        ),
      ),
    EWhile: ($) =>
      prec.right(
        PREC.CONTROL + 1,
        choice(
          seq(
            "while",
            "(",
            field("cond", $._Expr),
            ")",
            field("body", $._block_or_expr),
          ),
          seq(
            "do",
            field("body", $._block_or_expr),
            "while",
            "(",
            field("cond", $._Expr),
            ")",
          ),
        ),
      ),
    ESwitch: ($) =>
      prec(
        PREC.CONTROL,
        seq(
          "switch",
          field("subject", $._Expr),
          "{",
          optional(field("cases", $.cases)),
          optional(field("default", $.switch_default)),
          "}",
        ),
      ),
    cases: ($) => repeat1($.switch_case),
    // `case var x:` binds `x` as a capture. Precedence above a type annotation
    // so the trailing `:` is the case colon, not the start of `var x : T`.
    capture_variable: ($) =>
      prec(PREC.TYPE_ANNOTATION + 1, seq("var", field("name", $._identifier))),
    switch_case: ($) =>
      prec.dynamic(
        -1,
        seq(
          "case",
          field("patterns", commaSep1(choice($._Expr, $.capture_variable))),
          optional(seq("if", "(", field("guard", $._Expr), ")")),
          ":",
          field("body", repeat(seq($._Expr, optional($._semicolon)))),
        ),
      ),
    switch_default: ($) =>
      prec.right(
        1,
        seq(
          choice("default", seq("case", alias($.wildcard_pattern, ""))),
          ":",
          field("body", repeat(seq($._Expr, optional($._semicolon)))),
        ),
      ),
    ETry: ($) =>
      prec.right(
        seq(
          "try",
          $._block_or_expr,
          repeat1(
            seq(
              "catch",
              "(",
              field("name", $.identifier),
              optional($._type_annotation),
              ")",
              field("body", $._block_or_expr),
            ),
          ),
        ),
      ),
    ECheckType: ($) =>
      prec.right(
        PREC.TYPE_ANNOTATION,
        seq("(", field("expr", $._Expr), $._type_annotation, ")"),
      ),
    EMeta: ($) =>
      prec.right(
        PREC.PRIMARY,
        seq(repeat1($.MetaDataEntry), field("expr", $._Expr)),
        // seq(
        //   repeat1($.MetaDataEntry),
        //   field("expr", choice($._type_decl, $.EFunction, $.EVars)),
        // ),
      ),

    macro: ($) =>
      prec(
        PREC.MACRO,
        seq(
          "macro",
          choice($._Expr, $.ClassType, $.EnumType, $._type_annotation),
        ),
      ),
    reification: ($) =>
      prec(
        PREC.UNARY,
        choice(
          seq(
            "$",
            choice(
              token.immediate(/[a-zA-Z_][a-zA-Z0-9_]*/), // identifier
              seq(token.immediate("{"), $._Expr, "}"),
            ),
          ),
          seq(token(/\$e\{/), $._Expr, "}"),
          seq(token(/\$a\{/), commaSep($._Expr), "}"),
          seq(
            token(/\$b\{/),
            repeat(seq($._Expr, optional($._semicolon))),
            "}",
          ),
          seq(token(/\$i\{/), $.identifier, "}"),
          seq(token(/\$p\{/), commaSep($.identifier), "}"),
          seq(token(/\$v\{/), $._Expr, "}"),
        ),
      ),

    type_trace: ($) =>
      prec(PREC.CALL + 1, seq("$type", "(", field("type", $._Expr), ")")),

    // ------------------------------------------------------------------------

    TypePath: ($) =>
      seq(
        repeat(seq(field("pack", $.package_name), ".")),
        field("name", $._type_name),
        optional(field("sub", prec.left(PREC.PRIMARY, seq(".", $._identifier)))),
        optional(field("params", $._type_arguments)),
      ),

    ComplexType: ($) =>
      choice(
        $._base_type,
        prec.left(
          2,
          field("TIntersection", seq($.ComplexType, "&", $.ComplexType)),
        ),
        prec.right(
          1,
          field(
            "TFunction",
            seq(field("arg", $._base_type), "->", field("ret", $.ComplexType)),
          ),
        ),
      ),

    _base_type: ($) =>
      choice(
        //field("TPath", prec(PREC.PRIMARY, $.TypePath)),
        prec(PREC.PRIMARY, $.TypePath),
        $.TAnonymous,
        $._ct_paren, // ( T ) and function-arg lists: (), (T, U), (a:T, ?b:U)
        prec.right(seq("?", $._base_type)), // TOptional: ?Int
        prec.right(seq("...", $._base_type)), // TRest: ...Int -> haxe.Rest<Int>
      ),

    // Parenthesised type: a single (T) or a function-type argument list
    // (a:T, ?b:U). Whether it is a real arg list is decided by a following "->".
    _ct_paren: ($) =>
      seq("(", commaSep($._ct_fun_param), optional(","), ")"),
    _ct_fun_param: ($) => choice($._ct_fun_param_named, $.ComplexType),
    _ct_fun_param_named: ($) =>
      seq(
        optional($.optional),
        field("name", $._identifier),
        ":",
        field("type", $.ComplexType),
      ),

    TAnonymous: ($) =>
      prec.right(
        PREC.OBJECT_DECL,
        seq(
          "{",
          optional(repeat1(seq(">", field("extends", $.TypePath), ","))),
          // commaSep accepts zero Fields (the empty `{}` structure) and the
          // comma short form `{a:T, b:U}`. The trailing semicolon that closes a
          // typedef/var declaration is consumed by the enclosing rule, not here,
          // so an anonymous-structure var type does not swallow the field's `;`.
          choice(seq(commaSep($.Field), optional(",")), repeat($._class_field)),
          "}",
        ),
      ),
    Field: ($) =>
      seq(
        optional($.optional),
        field("name", $._identifier),
        ":",
        field("type", $.ComplexType),
      ),

    TypeParameter: ($) =>
      seq(
        field("name", $._type_name),
        optional(seq(":", field("constraint", $.ComplexType))),
        optional(seq("=", field("default", $.ComplexType))),
      ),

    MetaDataEntry: ($) =>
      prec.right(
        seq(
          "@",
          optional(token.immediate(":")),
          field(
            "name",
            seq(
              $._metadata_name_component,
              repeat(seq(token.immediate("."), $._metadata_name_component)),
            ),
          ),
          // Args `(` must immediately follow the name. With a space, the parens
          // belong to a following expression so `@:privateAccess (a.b)` is an
          // EMeta-prefixed expression, not a metadata argument list.
          optional(
            seq(token.immediate("("), field("params", commaSep($._Expr)), ")"),
          ),
        ),
      ),
    _metadata_name_component: ($) =>
      choice(
        $.identifier,
        ...RESERVED_KEYWORDS.map((kw) => alias(kw, $.identifier)),
      ),

    FunctionArg: ($) =>
      //ISSUE: without dynamic ECast fails
      prec.dynamic(
        1,
        seq(
          repeat($.MetaDataEntry),
          optional($.optional),
          optional(field("rest", $.rest)),
          // `_` is the wildcard keyword token; accept it as a parameter name so
          // `(_) -> e` is an arrow function, not a parenthesised wildcard.
          field("name", choice($._identifier, alias("_", $.identifier))),
          optional(field("type", $._type_annotation)),
          optional(seq("=", field("value", $._Expr))),
        ),
      ),

    // ------------------------------------------------------------------------

    _type_decl: ($) =>
      seq(
        field("meta", repeat($.MetaDataEntry)),
        choice($.AbstractType, $.ClassType, $.DefType, $.EnumType),
      ),

    AbstractType: ($) =>
      seq(
        // `extern` and `private` (via $.visibility) are the only non-enum
        // abstract rights the compiler's decl_flag_to_abstract_flag accepts
        // (src/syntax/parser.ml); `final` is rejected, so it is omitted here.
        optional(repeat1(choice($.visibility, "enum", "extern"))),
        "abstract",
        $._type_decl_signature,
        optional(seq("(", field("type", $.ComplexType), ")")),
        repeat(
          choice(
            field("from", seq("from", $.ComplexType)),
            field("to", seq("to", $.ComplexType)),
          ),
        ),
        "{",
        repeat($._class_field),
        "}",
      ),

    ClassType: ($) =>
      seq(
        optional(repeat1(choice($.visibility, "abstract", "extern", "final"))),
        field("kind", choice("class", "interface")),
        $._type_decl_signature,
        repeat(
          choice(
            seq("extends", field("extends", $.TypePath)),
            seq("implements", field("implements", $.TypePath)),
          ),
        ),
        "{",
        repeat($._class_field),
        "}",
      ),
    _class_field: ($) =>
      seq(repeat($.MetaDataEntry), choice($.ClassVar, $.ClassMethod)),
    ClassVar: ($) =>
      prec.left(
        PREC.ASSIGN,
        seq(
          optional(
            repeat1(
              choice(
                $.visibility,
                "abstract",
                "dynamic",
                "extern",
                "inline",
                "macro",
                "overload",
                "override",
                "static",
              ),
            ),
          ),
          choice("var", "final"),
          optional($.optional),
          field("name", $._identifier),
          optional($.property_accessor),
          optional($._type_annotation),
          optional(seq("=", $._Expr)),
          $._semicolon,
        ),
      ),
    property_accessor: ($) =>
      seq(
        "(",
        field("get", $.property_access),
        ",",
        field("set", $.property_access),
        ")",
      ),
    property_access: ($) =>
      choice(
        alias("default", $.default),
        alias("get", $.get),
        alias("set", $.set),
        alias("dynamic", $.dynamic),
        alias("never", $.never),
        $.null,
        $.identifier,
      ),
    ClassMethod: ($) =>
      prec.right(
        PREC.ASSIGN,
        seq(
          optional(
            repeat1(
              choice(
                $.visibility,
                "macro",
                "dynamic",
                "inline",
                "override",
                "abstract",
                "extern",
                "final",
                "overload",
                "static",
              ),
            ),
          ),
          $._function_decl,
          optional($._semicolon),
        ),
      ),
    DefType: ($) =>
      seq(
        optional(repeat1(choice($.visibility, "extern"))),
        "typedef",
        $._type_decl_signature,
        "=",
        field("type", $.ComplexType),
        optional($._semicolon),
      ),

    EnumType: ($) =>
      seq(
        optional(repeat1(choice($.visibility, "extern"))),
        "enum",
        $._type_decl_signature,
        "{",
        repeat($.EnumConstructor),
        "}",
      ),
    EnumConstructor: ($) =>
      seq(
        field("meta", repeat($.MetaDataEntry)),
        field("name", $.identifier),
        optional(field("params", $._type_params)),
        optional(field("args", $._function_args)),
        $._semicolon,
      ),

    _function_decl: ($) =>
      seq(
        "function",
        optional(
          field(
            "name",
            choice($._identifier, alias("new", $.identifier), $._soft_keyword_ident),
          ),
        ),
        // Type parameters may appear without a name, e.g. the anonymous generic
        // function expression `function<T>(a:T):T {}` used as a metadata arg.
        optional(field("params", $._type_params)),
        $._function_args,
        optional(field("ret", $._type_annotation)),
        choice(field("body", $._block_or_expr), $._semicolon),
      ),

    _type_decl_signature: ($) =>
      seq(field("name", $._type_name), optional($._type_params)),

    // ------------------------------------------------------------------------

    Int: (_) =>
      choice(
        /0x[a-fA-F\d][a-fA-F\d_]*_?[iu]\d+/,
        /0x[a-fA-F\d][a-fA-F\d_]*/,
        /0b[01][01_]*_?[iu]\d+/,
        /0b[01][01_]*/,
        // No octal: the Haxe lexer (src/syntax/lexer.ml) accepts only decimal,
        // 0x and 0b integer forms; `0o...` is intentionally rejected.
        /\d[\d_]*_?[iu]\d+/,
        /\d[\d_]*/,
      ),

    Float: ($) =>
      choice(
        /\d[\d_]*\.\d[\d_]*([eE][+-]?\d[\d_]*)?_?f\d+/,
        /\d[\d_]*\.\d[\d_]*([eE][+-]?\d[\d_]*)?/,
        /\d[\d_]*\.[eE][+-]?\d[\d_]*_?f\d+/,
        /\d[\d_]*\.[eE][+-]?\d[\d_]*/,
        /\.[\d_]+([eE][+-]?\d[\d_]*)?_?f\d+/,
        /\.[\d_]+([eE][+-]?\d[\d_]*)?/,
        /\d[\d_]+[eE][+-]?\d[\d_]*_?f\d+/,
        /\d[\d_]+[eE][+-]?\d[\d_]*/,
        /\d[\d_]*_?f\d+/,
        // Trailing-dot float `N.` (e.g. `0.`, `1000.`). An external token so the
        // lexer can look past the dot and decline when the next char begins an
        // interval `...` or a field access, which a regex (no lookahead) cannot.
        $._float_trailing_dot,
      ),

    String: ($) =>
      choice(
        seq(
          "'",
          repeat(
            choice(
              alias(token.immediate(prec(1, /[^'\\$]+/)), $.fragment),
              // `$$` is the literal-dollar escape in interpolated strings; it
              // must out-munch the single-`$` interpolation start.
              alias(token.immediate(prec(2, "$$")), $.escape_sequence),
              $.escape_sequence,
              $.interpolation,
            ),
          ),
          "'",
        ),
        seq(
          '"',
          repeat(
            choice(
              alias(token.immediate(prec(1, /[^"\\]+/)), $.fragment),
              $.escape_sequence,
            ),
          ),
          '"',
        ),
      ),
    escape_sequence: () => token.immediate(seq("\\", /./)),
    interpolation: ($) =>
      seq(
        "$",
        choice(
          seq(token.immediate("{"), $._Expr, "}"),
          alias(token.immediate(/[a-zA-Z_][a-zA-Z0-9_]*/), $.identifier),
        ),
      ),

    Regexp: (_) =>
      seq("~/", repeat(choice(/[^/\\\n]/, /\\./)), "/", /[gimsu]*/),

    true: (_) => "true",
    false: (_) => "false",
    null: (_) => "null",
    this: (_) => "this",
    super: (_) => "super",
    rest: (_) => "...",

    // ------------------------------------------------------------------------

    _dot_path: ($) => dotSep1($.package_name),

    _type_annotation: ($) =>
      prec(PREC.TYPE_ANNOTATION, seq(":", field("type", $.ComplexType))),

    _type_params: ($) => seq("<", commaSep1($.TypeParameter), ">"),
    _type_arguments: ($) => seq("<", commaSep($._type_argument), ">"),
    // A type argument is a type or, for const type parameters, a literal/const
    // expression (e.g. `Vector<3>`, `Foo<"x">`, `Foo<true>`, `Foo<[1,2]>`).
    _type_argument: ($) =>
      choice(
        $.ComplexType,
        $.Int,
        $.Float,
        $.String,
        $.true,
        $.false,
        $.null,
        $.Regexp,
        $.EArrayDecl,
        seq("-", choice($.Int, $.Float)),
        seq(choice("!", "~"), choice($.Int, $.Float, $.String)),
      ),

    _function_args: ($) =>
      seq("(", field("args", commaSep($.FunctionArg)), ")"),

    // ------------------------------------------------------------------------

    visibility: (_) => choice("public", "private"),
    // Field modifier keywords, usable as a lone `#if ... #end` conditional body.
    // Higher precedence so a bare modifier token inside a conditional resolves
    // to this rule rather than starting a class field / inline-call expression.
    modifier: (_) =>
      prec(
        30,
        choice(
          "inline",
          "static",
          "final",
          "dynamic",
          "override",
          "extern",
          "overload",
        ),
      ),

    identifier: (_) => /[a-zA-Z_][a-zA-Z0-9_]*/,
    dollar_identifier: (_) => /\$[a-zA-Z_][a-zA-Z0-9_]*/,
    _identifier: ($) =>
      choice($.identifier, alias($.dollar_identifier, $.identifier)),
    // `as` is a reserved keyword (used in `import ... as`), but the compiler also
    // accepts it as a plain identifier for fields, methods, variables and calls.
    // (`from`/`to` are not reserved, so they already work as identifiers.)
    _soft_keyword_ident: ($) => alias("as", $.identifier),
    package_name: (_) => /[a-z_][a-zA-Z0-9_]*/,
    // Higher lexical precedence so an uppercase word prefers `type_name` over
    // `identifier` in states where both are valid (e.g. unnamed function-type
    // args `(Int, String) -> T`); lowercase words still fall back to identifier.
    type_name: (_) => token(prec(1, /[A-Z][a-zA-Z0-9_]*/)),
    _type_name: ($) =>
      choice($.type_name, alias($.dollar_identifier, $.type_name)),

    optional: (_) => "?",
    wildcard: (_) => "*",
    wildcard_pattern: (_) => "_",
    _semicolon: (_) => ";",

    comment: ($) => choice($.line_comment, $.block_comment),
    line_comment: (_) => token(seq("//", /[^\n]*/)),
    block_comment: (_) => token(seq("/*", /[^*]*\*+([^/*][^*]*\*+)*\//)),

    // ------------------------------------------------------------------------

    conditional: ($) =>
      prec.right(
        PREC.CONDITIONAL,
        seq(
          "#if",
          $.compile_condition,
          repeat($._conditional_body),
          repeat($.conditional_elseif),
          optional($.conditional_else),
          $.conditional_end,
        ),
      ),
    // Conditional in expression position, whose branches are full expressions.
    // Distinct from the floating `conditional` extra so it can fill a required
    // expression slot (e.g. a for-loop iterable or a var initializer).
    EConditional: ($) =>
      prec.right(
        PREC.CONDITIONAL,
        seq(
          "#if",
          $.compile_condition,
          $._Expr,
          repeat(seq("#elseif", $.compile_condition, $._Expr)),
          optional(seq("#else", $._Expr)),
          $.conditional_end,
        ),
      ),
    compile_condition: ($) =>
      choice(
        $.identifier,
        alias("macro", $.identifier),
        $.Int,
        $.Float,
        $.String,
        seq("(", $.compile_condition, ")"),
        prec.left(7, seq($.compile_condition, ".", $.identifier)),
        // Function-call form, e.g. `version("1.10.0")` in `#if (hl_ver >= version(...))`.
        prec.left(7, seq($.compile_condition, "(", commaSep($.compile_condition), ")")),
        prec.right(6, seq("!", $.compile_condition)),
        prec.left(
          5,
          seq($.compile_condition, choice("*", "/", "%"), $.compile_condition),
        ),
        prec.left(
          4,
          seq($.compile_condition, choice("+", "-"), $.compile_condition),
        ),
        prec.left(
          3,
          seq(
            $.compile_condition,
            choice(">", ">=", "<", "<=", "==", "!="),
            $.compile_condition,
          ),
        ),
        prec.left(2, seq($.compile_condition, "&&", $.compile_condition)),
        prec.left(1, seq($.compile_condition, "||", $.compile_condition)),
      ),
    _conditional_body: ($) =>
      choice(
        // ISSUE: adding everything sucks
        $.import,
        $.using,
        $._class_field,
        $._expr_statement,
        $._type_decl,
        $.visibility,
        // Lone field-modifier keywords, e.g. `public #if !cppia inline #end function`.
        $.modifier,
        // Class-heritage clauses, e.g. `extends D #if !flag implements Dynamic #end`.
        seq("extends", field("extends", $.TypePath)),
        seq("implements", field("implements", $.TypePath)),
        $.conditional_error,
        repeat1($.MetaDataEntry),
      ),
    conditional_elseif: ($) =>
      seq("#elseif", $.compile_condition, repeat($._conditional_body)),
    conditional_else: ($) => seq("#else", repeat($._conditional_body)),
    conditional_error: ($) => seq("#error", $.String),
    conditional_end: (_) => "#end",
  },
});
