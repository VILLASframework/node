// SPDX-FileCopyrightText: 2014-2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
// SPDX-License-Identifier: Apache-2.0
// Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>

function expandDiscriminatorPreprocessor() {
  return {
    Schema: {
      leave(schema, ctx) {
        const key = "x-villas-plugin"
        const discriminator = schema.discriminator
        if (discriminator !== undefined && discriminator[key] !== undefined) {
          const refs = Object.values(discriminator.mapping).map(ref => ({ "$ref": ref }))
          if (Array.isArray(schema.type) && schema.type.includes("string")) {
            delete schema.discriminator

            schema.anyOf = [
              {
                "title": "object",
                "type": "object",
                "discriminator": discriminator,
                "anyOf": refs,
              },
              {
                "title": "string",
                "type": "string",
                "enum": Object.keys(discriminator.mapping),
              },
            ]
          } else {
            schema.anyOf = refs;
          }

          delete discriminator[key]
        }
      }
    }
  }
}

function isNullableType(type) {
  if (type === "null") {
    return true
  }

  if (Array.isArray(type)) {
    return type.includes("null")
  }

  return false
}

function checkNullable(schema, needsDefault, ctx) {
  if (typeof schema === "boolean") {
    if (schema && needsDefault) {
      ctx.report({
        message: `Nullable property \`${ctx.key}\` should specify an explict \`null\` default value.`,
        location: ctx.location,
        from: ctx.origin,
        suggest: [
          "to add a `null` default value.",
        ]
      })
    }

    return schema
  }

  if (schema.$ref !== undefined) {
    const resolved = ctx.resolve(schema)

    if (!["boolean", "object"].includes(typeof resolved.node)) {
      ctx.report({
        message: `Could not resolve reference \`${schema.$ref}\` for property \`${ctx.key}\``,
        location: ctx.location,
        from: ctx.origin,
        suggest: [
          "Restrict the type to be non-nullable.",
          "Make the default value `null`.",
        ]
      })
    }

    const isNullable = checkNullable(resolved.node, needsDefault && schema.default !== null, {
      ...ctx,
      resolve: (schemaOrRef, resolveFrom = resolved.location.source.absoluteRef) => ctx.resolve(schemaOrRef, resolveFrom),
      location: resolved.location,
      origin: ctx.origin ?? ctx.location,
    })

    if (needsDefault && isNullable && schema.default !== undefined && schema.default !== null) {
      ctx.report({
        message: `Nullable property \`${ctx.key}\` has a non-null default value: ${JSON.stringify(schema.default)}`,
        location: ctx.location,
        from: ctx.origin,
        suggest: [
          "Restrict the type to be non-nullable.",
          "Make the default value `null`.",
        ]
      })
    }

    return isNullable
  }

  if (schema.const !== undefined && schema.const !== null) {
    return false
  }

  if (schema.enum !== undefined && !schema.enum.includes(null)) {
    return false
  }

  if (schema.type !== undefined && !isNullableType(schema.type)) {
    return false
  }

  if (schema.not !== undefined && checkNullable(schema.not, false, ctx)) {
    return false
  }

  if (schema.if !== undefined && checkNullable(schema.if, false, ctx)) {
    if (schema.then !== undefined && !checkNullable(schema.then, needsDefault, ctx)) {
      return false
    }
  } else {
    if (schema.else !== undefined && !checkNullable(schema.else, needsDefault, ctx)) {
      return false
    }
  }

  if (schema.anyOf !== undefined && !schema.anyOf.some((subschema) => checkNullable(subschema, false, ctx))) {
    return false
  }

  if (schema.allOf !== undefined && !schema.allOf.every((subschema) => checkNullable(subschema, false, ctx))) {
    return false
  }

  if (schema.oneOf !== undefined && schema.oneOf.filter((subschema) => checkNullable(subschema, false, ctx)).length !== 1) {
    return false
  }

  if (needsDefault && schema.default !== null) {
    ctx.report({
      message: `Nullable property \`${ctx.key}\` has a non-null default value: ${JSON.stringify(schema.default)}`,
      location: ctx.location,
      from: ctx.origin,
      suggest: [
        "to restrict the type to be non-nullable.",
        "to make the default value `null`.",
      ]
    })
  }

  return true
}

function nullableDefaultRule() {
  return {
    SchemaProperties: {
      enter(properties, ctx) {
        const required = new Set(ctx.parent.required || [])
        for (const [name, property] of Object.entries(properties)) {
          checkNullable(property, !required.has(name), {
            ...ctx,
            key: name,
            location: ctx.location.child(name),
          })
        }
      }
    }
  }
}

function nullableOptionalRule() {
  return {
    SchemaProperties: {
      enter(properties, ctx) {
        const required = new Set(ctx.parent.required || [])
        for (const [name, property] of Object.entries(properties)) {
          if (required.has(name) && checkNullable(property, false, {
            ...ctx,
            key: name,
            location: ctx.location.child(name),
          })) {
            ctx.report({
              message: `Nullable property \`${name}\` should not be required.`,
              location: ctx.location.child(name),
              from: ctx.location,
            })
          }
        }
      }
    }
  }
}

function additionalPropertiesRule() {
  return {
    Schema: {
      enter(schema, ctx) {
        if (typeof schema !== "object") return

        if (schema.properties !== undefined && schema.additionalProperties === undefined) {
          ctx.report({
            message: "Missing `additionalProperties` for object-like schema.",
            location: ctx.location,
          })
        }
      }
    }
  }
}

function additionalItemsRule() {
  return {
    Schema: {
      enter(schema, ctx) {
        if (typeof schema !== "object") return

        if (Array.isArray(schema.items) && schema.additionalItems === undefined) {
          ctx.report({
            message: "Missing `additionalItems` for tuple-like schema.",
            location: ctx.location,
          })
        }
      }
    }
  }
}

export default {
  id: 'villas',

  preprocessors: {
    oas3: {
      'expand-discriminator': expandDiscriminatorPreprocessor,
    },
  },

  rules: {
    oas3: {
      'nullable-default': nullableDefaultRule,
      'nullable-optional': nullableOptionalRule,
      'additional-properties': additionalPropertiesRule,
      'additional-items': additionalItemsRule,
    },
  },
}
