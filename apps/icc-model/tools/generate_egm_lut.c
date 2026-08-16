/*
 * Host-only generator for the FlexPRET one-dimensional relative-potential
 * EGM lookup table. Target code never evaluates this floating-point model.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELATIVE_MIN_UM (-24000)
#define RELATIVE_MAX_UM 24000
#define RELATIVE_STEP_UM 60
#define ENTRY_COUNT 801U
#define ELECTRODE_HEIGHT_UM 1000
#define EGM_SCALE 10000000
#define DIPOLE_MOMENT 18.0
#define LONGITUDINAL_WEIGHT 1.0
#define TRANSVERSE_WEIGHT 0.1

typedef struct {
    int32_t values[ENTRY_COUNT];
    double maximum_absolute_error;
    double maximum_meaningful_relative_error;
    int32_t worst_absolute_position_um;
    int32_t worst_relative_position_um;
    int32_t maximum_absolute_value;
} GenerationResult;

static double reference_potential(int32_t oriented_relative_um)
{
    const double relative_mm = (double)oriented_relative_um / 1000.0;
    const double height_mm = (double)ELECTRODE_HEIGHT_UM / 1000.0;
    const double distance_squared =
        relative_mm * relative_mm + height_mm * height_mm;
    const double distance_cubed =
        distance_squared * sqrt(distance_squared);

    return DIPOLE_MOMENT *
        (LONGITUDINAL_WEIGHT * relative_mm -
         TRANSVERSE_WEIGHT * height_mm) /
        distance_cubed;
}

static int32_t quantize(double potential)
{
    const double scaled = potential * (double)EGM_SCALE;
    const long rounded = lround(scaled);

    if (rounded < INT32_MIN || rounded > INT32_MAX) {
        fprintf(stderr, "quantized EGM value exceeds int32_t\n");
        exit(EXIT_FAILURE);
    }
    return (int32_t)rounded;
}

static FILE *open_output(
    const char *directory,
    const char *filename,
    char *path,
    size_t path_size)
{
    const int written = snprintf(path, path_size, "%s/%s", directory, filename);
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "output path is too long\n");
        exit(EXIT_FAILURE);
    }

    FILE *output = fopen(path, "w");
    if (output == NULL) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return output;
}

static GenerationResult generate_values(void)
{
    GenerationResult result = {0};
    const double meaningful_threshold = 100.0 / (double)EGM_SCALE;

    for (uint32_t index = 0U; index < ENTRY_COUNT; ++index) {
        const int32_t position_um =
            RELATIVE_MIN_UM + (int32_t)index * RELATIVE_STEP_UM;
        const double reference = reference_potential(position_um);
        const int32_t stored = quantize(reference);
        const double reconstructed = (double)stored / (double)EGM_SCALE;
        const double absolute_error = fabs(reconstructed - reference);
        const double absolute_value = fabs((double)stored);

        result.values[index] = stored;
        if (absolute_value > (double)INT32_MAX) {
            fprintf(stderr, "absolute EGM value cannot be represented\n");
            exit(EXIT_FAILURE);
        }
        if (absolute_value > (double)result.maximum_absolute_value) {
            result.maximum_absolute_value = (int32_t)absolute_value;
        }
        if (absolute_error > result.maximum_absolute_error) {
            result.maximum_absolute_error = absolute_error;
            result.worst_absolute_position_um = position_um;
        }
        if (fabs(reference) >= meaningful_threshold) {
            const double relative_error = absolute_error / fabs(reference);
            if (relative_error > result.maximum_meaningful_relative_error) {
                result.maximum_meaningful_relative_error = relative_error;
                result.worst_relative_position_um = position_um;
            }
        }
    }
    return result;
}

static void write_header(const char *directory, const GenerationResult *result)
{
    char path[1024];
    FILE *output = open_output(directory, "egm_relative_lut.h", path, sizeof(path));

    fprintf(output,
        "#ifndef ICC_MODEL_GENERATED_EGM_RELATIVE_LUT_H\n"
        "#define ICC_MODEL_GENERATED_EGM_RELATIVE_LUT_H\n\n"
        "#include <stdint.h>\n\n"
        "#define EGM_LUT_RELATIVE_MIN_UM (%d)\n"
        "#define EGM_LUT_RELATIVE_MAX_UM %d\n"
        "#define EGM_LUT_POSITION_STEP_UM %dU\n"
        "#define EGM_LUT_ENTRY_COUNT %uU\n"
        "#define EGM_LUT_ELECTRODE_HEIGHT_UM %dU\n"
        "#define EGM_LUT_SCALE %d\n"
        "#define EGM_LUT_MAX_ABS_VALUE %d\n"
        "#define EGM_LUT_TABLE_BYTES %uU\n\n"
        "static const int32_t kEgmRelativePotential[EGM_LUT_ENTRY_COUNT] = {\n",
        RELATIVE_MIN_UM,
        RELATIVE_MAX_UM,
        RELATIVE_STEP_UM,
        ENTRY_COUNT,
        ELECTRODE_HEIGHT_UM,
        EGM_SCALE,
        result->maximum_absolute_value,
        ENTRY_COUNT * (unsigned)sizeof(int32_t));

    for (uint32_t index = 0U; index < ENTRY_COUNT; ++index) {
        fprintf(output, "    %d%s", result->values[index],
            index + 1U == ENTRY_COUNT ? "\n" : ",");
        if (index + 1U != ENTRY_COUNT && (index + 1U) % 8U == 0U) {
            fputc('\n', output);
        } else if (index + 1U != ENTRY_COUNT) {
            fputc(' ', output);
        }
    }
    fprintf(output, "};\n\n#endif\n");
    fclose(output);
    printf("generated %s\n", path);
}

static void write_csv(const char *directory, const GenerationResult *result)
{
    char path[1024];
    FILE *output = open_output(directory, "egm_relative_lut.csv", path, sizeof(path));

    fprintf(output,
        "oriented_relative_position_um,reference_potential,scaled_integer,"
        "reconstructed_potential,quantization_error,absolute_error\n");
    for (uint32_t index = 0U; index < ENTRY_COUNT; ++index) {
        const int32_t position_um =
            RELATIVE_MIN_UM + (int32_t)index * RELATIVE_STEP_UM;
        const double reference = reference_potential(position_um);
        const double reconstructed =
            (double)result->values[index] / (double)EGM_SCALE;
        const double error = reconstructed - reference;
        fprintf(output, "%d,%.17g,%d,%.17g,%.17g,%.17g\n",
            position_um, reference, result->values[index], reconstructed,
            error, fabs(error));
    }
    fclose(output);
    printf("generated %s\n", path);
}

static void write_report(const char *directory, const GenerationResult *result)
{
    char path[1024];
    FILE *output = open_output(
        directory, "EGM_LUT_GENERATION_REPORT.md", path, sizeof(path));

    fprintf(output,
        "# Relative-Potential EGM LUT Generation Report\n\n"
        "- Relative-position range: `%d` to `%d` um\n"
        "- Spatial step: `%d` um\n"
        "- Entry count: `%u`\n"
        "- Entry type: `int32_t`\n"
        "- Table storage: `%u` bytes\n"
        "- Electrode height: `%d` um\n"
        "- Integer scale: `%d` units per model-potential unit\n"
        "- Maximum absolute stored entry: `%d`\n"
        "- Four-dipole conservative absolute sum: `%d`\n"
        "- Maximum absolute quantization error: `%.17g` at `%d` um\n"
        "- Maximum meaningful relative error: `%.17g` at `%d` um\n"
        "- Meaningful-relative-error threshold: `1e-5` potential units\n"
        "- Absolute-error acceptance bound: `5.00001e-8` potential units\n\n"
        "The table depends only on oriented relative position and fixed physical "
        "EGM parameters. It does not depend on timestep, path identity, cell "
        "number, or absolute electrode position.\n",
        RELATIVE_MIN_UM, RELATIVE_MAX_UM, RELATIVE_STEP_UM, ENTRY_COUNT,
        ENTRY_COUNT * (unsigned)sizeof(int32_t), ELECTRODE_HEIGHT_UM,
        EGM_SCALE, result->maximum_absolute_value,
        result->maximum_absolute_value * 4,
        result->maximum_absolute_error, result->worst_absolute_position_um,
        result->maximum_meaningful_relative_error,
        result->worst_relative_position_um);
    fclose(output);
    printf("generated %s\n", path);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }

    if ((RELATIVE_MAX_UM - RELATIVE_MIN_UM) % RELATIVE_STEP_UM != 0 ||
        (uint32_t)((RELATIVE_MAX_UM - RELATIVE_MIN_UM) /
            RELATIVE_STEP_UM + 1) != ENTRY_COUNT) {
        fprintf(stderr, "relative-position metadata is inconsistent\n");
        return EXIT_FAILURE;
    }

    const GenerationResult result = generate_values();
    if (result.maximum_absolute_value > INT32_MAX / 4) {
        fprintf(stderr, "four simultaneous dipoles can overflow int32_t\n");
        return EXIT_FAILURE;
    }

    write_header(argv[1], &result);
    write_csv(argv[1], &result);
    write_report(argv[1], &result);
    return EXIT_SUCCESS;
}
