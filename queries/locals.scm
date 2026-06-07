; Scopes ----------------------------------------------------------------------

[
  (EBlock)
  (EFunction)
  (EArrowFunction)
  (EFor)
  (ETry)
] @local.scope

; Definitions ------------------------------------------------------------------

(FunctionArg
  name: (identifier) @local.definition)

(EVars
  name: (identifier) @local.definition)

(EFor
  var: (identifier) @local.definition)
(EFor
  key: (identifier) @local.definition)
(EFor
  value: (identifier) @local.definition)

(ETry
  name: (identifier) @local.definition)

(capture_variable
  name: (identifier) @local.definition)

; References -------------------------------------------------------------------

(identifier) @local.reference
