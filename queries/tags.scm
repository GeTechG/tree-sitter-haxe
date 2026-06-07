; Type declarations -----------------------------------------------------------

(ClassType
  name: (type_name) @name) @definition.class

(EnumType
  name: (type_name) @name) @definition.enum

(AbstractType
  name: (type_name) @name) @definition.class

(DefType
  name: (type_name) @name) @definition.type

; Methods and functions --------------------------------------------------------

(ClassMethod
  name: (identifier) @name) @definition.method

(EFunction
  name: (identifier) @name) @definition.function

; Fields -----------------------------------------------------------------------

(ClassVar
  name: (identifier) @name) @definition.field

(Field
  name: (identifier) @name) @definition.field

; Enum constructors ------------------------------------------------------------

(EnumConstructor
  name: (identifier) @name) @definition.enum
