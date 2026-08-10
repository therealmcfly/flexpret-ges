/*
 * Host-side generator for the first FlexPRET EGM lookup-table milestone.
 *
 * This program intentionally uses the same single-precision operation order
 * as iccnet-core/src/egm.c. It is a development tool, not target code.
 *
 * Initial reference configuration:
 *   path A (0, 0) -> B (6, 0) mm
 *   delay 1000 ms
 *   electrode at grid row 0, column 1, height 1 mm
 *   dipole parameters 18.0, 1.0, 0.1
 *
 * It generates lookup headers and CSV evidence for all supported timesteps.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EGM_PATH_DELAY_MS 1000U
#define EGM_PATH_GAP_MM 6U
#define EGM_ELECTRODE_ROW 0U
#define EGM_ELECTRODE_COL 1U
#define EGM_ELECTRODE_HEIGHT_MM 1U

#define EGM_DIPOLE_MOMENT 18.0f
#define EGM_LONGITUDINAL_WEIGHT 1.0f
#define EGM_TRANSVERSE_WEIGHT 0.1f
#define EGM_DIPOLE_EPSILON 1.0e-6f

#define EGM_DIRECTION_COUNT 2U
#define EGM_DIRECTION_A_TO_B 0U
#define EGM_DIRECTION_B_TO_A 1U
#define EGM_MAX_SIMULTANEOUS_PATHS 4U
#define EGM_SCALE_HEADROOM 0.80

typedef struct {
    float dipole_x_mm;
    float dipole_y_mm;
    float potential;
} EgmReferenceSample;

typedef struct {
    uint32_t timestep_ms;
    uint32_t step_count;
    float minimum_potential;
    float maximum_potential;
    double maximum_absolute_error;
    double rmse;
} EgmTimestepStatistics;

static const uint32_t kSupportedTimestepsMs[] = {
    200U, 100U, 50U, 20U, 10U
};

static EgmReferenceSample reference_sample(
    uint32_t direction,
    uint32_t elapsed_ms)
{
    const float gap_mm = (float)EGM_PATH_GAP_MM;
    const float a_x_mm = 0.0f;
    const float a_y_mm = 0.0f;
    const float b_x_mm = gap_mm;
    const float b_y_mm = 0.0f;
    const float start_x_mm = direction == EGM_DIRECTION_A_TO_B
        ? a_x_mm : b_x_mm;
    const float start_y_mm = direction == EGM_DIRECTION_A_TO_B
        ? a_y_mm : b_y_mm;
    const float end_x_mm = direction == EGM_DIRECTION_A_TO_B
        ? b_x_mm : a_x_mm;
    const float end_y_mm = direction == EGM_DIRECTION_A_TO_B
        ? b_y_mm : a_y_mm;
    const float path_dx = end_x_mm - start_x_mm;
    const float path_dy = end_y_mm - start_y_mm;
    const float path_length_mm = sqrtf(
        path_dx * path_dx + path_dy * path_dy);
    const float direction_x = path_dx / path_length_mm;
    const float direction_y = path_dy / path_length_mm;
    const float velocity_mm_s =
        path_length_mm * 1000.0f / (float)EGM_PATH_DELAY_MS;
    const float elapsed_s = (float)elapsed_ms * 1.0e-3f;
    float travelled_mm = velocity_mm_s * elapsed_s;

    if (travelled_mm > path_length_mm) {
        travelled_mm = path_length_mm;
    }

    EgmReferenceSample sample;
    sample.dipole_x_mm = start_x_mm + travelled_mm * direction_x;
    sample.dipole_y_mm = start_y_mm + travelled_mm * direction_y;

    const float electrode_x_mm =
        (float)EGM_ELECTRODE_COL * gap_mm;
    const float electrode_y_mm =
        (float)EGM_ELECTRODE_ROW * gap_mm;
    const float electrode_dx = electrode_x_mm - sample.dipole_x_mm;
    const float electrode_dy = electrode_y_mm - sample.dipole_y_mm;
    const float along_path_mm =
        electrode_dx * direction_x + electrode_dy * direction_y;
    const float perpendicular_x =
        electrode_dx - along_path_mm * direction_x;
    const float perpendicular_y =
        electrode_dy - along_path_mm * direction_y;
    const float height_mm = (float)EGM_ELECTRODE_HEIGHT_MM;
    const float perpendicular_mm = sqrtf(
        perpendicular_x * perpendicular_x +
        perpendicular_y * perpendicular_y +
        height_mm * height_mm);
    const float distance_squared =
        along_path_mm * along_path_mm +
        perpendicular_mm * perpendicular_mm;

    if (distance_squared <= EGM_DIPOLE_EPSILON) {
        sample.potential = 0.0f;
        return sample;
    }

    const float distance_cubed =
        distance_squared * sqrtf(distance_squared);
    const float longitudinal =
        EGM_LONGITUDINAL_WEIGHT * along_path_mm / distance_cubed;
    const float transverse =
        -EGM_TRANSVERSE_WEIGHT * perpendicular_mm / distance_cubed;
    sample.potential =
        (longitudinal + transverse) * EGM_DIPOLE_MOMENT;
    return sample;
}

static int32_t quantize(float potential, int32_t scale)
{
    const double scaled = (double)potential * (double)scale;
    const long rounded = lround(scaled);

    if (rounded < INT32_MIN || rounded > INT32_MAX) {
        fprintf(stderr, "quantized EGM value exceeds int32_t\n");
        exit(EXIT_FAILURE);
    }

    return (int32_t)rounded;
}

static bool join_path(
    char *destination,
    size_t destination_size,
    const char *directory,
    const char *filename)
{
    const int written = snprintf(
        destination, destination_size, "%s/%s", directory, filename);
    return written >= 0 && (size_t)written < destination_size;
}

static FILE *open_output(
    const char *directory,
    const char *filename,
    char *resolved_path,
    size_t resolved_path_size)
{
    if (!join_path(
            resolved_path, resolved_path_size, directory, filename)) {
        fprintf(stderr, "output path is too long: %s/%s\n",
            directory, filename);
        exit(EXIT_FAILURE);
    }

    FILE *output = fopen(resolved_path, "w");
    if (output == NULL) {
        fprintf(stderr, "cannot open %s: %s\n",
            resolved_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return output;
}

static float find_maximum_absolute_reference(void)
{
    float maximum = 0.0f;

    for (size_t timestep_index = 0U;
         timestep_index < sizeof(kSupportedTimestepsMs) /
             sizeof(kSupportedTimestepsMs[0]);
         ++timestep_index) {
        const uint32_t timestep_ms =
            kSupportedTimestepsMs[timestep_index];

        if (EGM_PATH_DELAY_MS % timestep_ms != 0U) {
            fprintf(stderr,
                "path delay %u is not divisible by timestep %u\n",
                EGM_PATH_DELAY_MS, timestep_ms);
            exit(EXIT_FAILURE);
        }

        const uint32_t step_count = EGM_PATH_DELAY_MS / timestep_ms;
        for (uint32_t direction = 0U;
             direction < EGM_DIRECTION_COUNT;
             ++direction) {
            for (uint32_t step = 0U; step < step_count; ++step) {
                const EgmReferenceSample sample = reference_sample(
                    direction, step * timestep_ms);
                const float magnitude = fabsf(sample.potential);
                if (magnitude > maximum) {
                    maximum = magnitude;
                }
            }
        }
    }

    return maximum;
}

static int32_t choose_common_decimal_scale(float maximum_absolute_reference)
{
    int32_t scale = 1;
    const double accumulator_limit =
        (double)INT32_MAX * EGM_SCALE_HEADROOM;

    while (scale <= INT32_MAX / 10) {
        const int32_t candidate = scale * 10;
        const double maximum_accumulator =
            (double)maximum_absolute_reference *
            (double)candidate *
            (double)EGM_MAX_SIMULTANEOUS_PATHS;

        if (maximum_accumulator > accumulator_limit) {
            break;
        }
        scale = candidate;
    }

    return scale;
}

static void write_header(
    const char *output_directory,
    uint32_t timestep_ms,
    int32_t scale,
    EgmTimestepStatistics *statistics)
{
    char filename[64];
    char output_path[512];
    (void)snprintf(filename, sizeof(filename),
        "egm_lut_%ums.h", timestep_ms);
    FILE *output = open_output(
        output_directory, filename, output_path, sizeof(output_path));

    const uint32_t step_count = EGM_PATH_DELAY_MS / timestep_ms;
    fprintf(output,
        "#ifndef ICC_MODEL_GENERATED_EGM_LUT_%uMS_H\n"
        "#define ICC_MODEL_GENERATED_EGM_LUT_%uMS_H\n\n"
        "#include <stdint.h>\n\n"
        "/* Generated by tools/generate_egm_lut.c. Do not edit. */\n"
        "#define EGM_LUT_TIMESTEP_MS %uU\n"
        "#define EGM_LUT_DELAY_MS %uU\n"
        "#define EGM_LUT_GAP_MM %uU\n"
        "#define EGM_LUT_ELECTRODE_ROW %uU\n"
        "#define EGM_LUT_ELECTRODE_COL %uU\n"
        "#define EGM_LUT_ELECTRODE_HEIGHT_MM %uU\n"
        "#define EGM_LUT_STEP_COUNT %uU\n"
        "#define EGM_LUT_SCALE %d\n"
        "#define EGM_LUT_DIRECTION_A_TO_B 0U\n"
        "#define EGM_LUT_DIRECTION_B_TO_A 1U\n\n"
        "static const int32_t kEgmPath0Lookup[2][%u] = {\n",
        timestep_ms, timestep_ms,
        timestep_ms, EGM_PATH_DELAY_MS, EGM_PATH_GAP_MM,
        EGM_ELECTRODE_ROW, EGM_ELECTRODE_COL,
        EGM_ELECTRODE_HEIGHT_MM, step_count, scale, step_count);

    statistics->timestep_ms = timestep_ms;
    statistics->step_count = step_count;
    statistics->minimum_potential = INFINITY;
    statistics->maximum_potential = -INFINITY;
    statistics->maximum_absolute_error = 0.0;
    double squared_error_sum = 0.0;
    uint32_t sample_count = 0U;

    for (uint32_t direction = 0U;
         direction < EGM_DIRECTION_COUNT;
         ++direction) {
        fprintf(output, "    { /* %s */\n        ",
            direction == EGM_DIRECTION_A_TO_B ? "A to B" : "B to A");

        for (uint32_t step = 0U; step < step_count; ++step) {
            const EgmReferenceSample sample = reference_sample(
                direction, step * timestep_ms);
            const int32_t integer_value = quantize(sample.potential, scale);
            const double reconstructed =
                (double)integer_value / (double)scale;
            const double error =
                reconstructed - (double)sample.potential;
            const double absolute_error = fabs(error);

            if (sample.potential < statistics->minimum_potential) {
                statistics->minimum_potential = sample.potential;
            }
            if (sample.potential > statistics->maximum_potential) {
                statistics->maximum_potential = sample.potential;
            }
            if (absolute_error > statistics->maximum_absolute_error) {
                statistics->maximum_absolute_error = absolute_error;
            }
            squared_error_sum += error * error;
            ++sample_count;

            fprintf(output, "%d", integer_value);
            if (step + 1U != step_count) {
                fprintf(output, ",%s",
                    (step + 1U) % 8U == 0U ? "\n        " : " ");
            }
        }

        fprintf(output, "\n    }%s\n",
            direction + 1U == EGM_DIRECTION_COUNT ? "" : ",");
    }

    statistics->rmse = sqrt(
        squared_error_sum / (double)sample_count);

    fprintf(output, "};\n\n#endif\n");
    fclose(output);
    printf("generated %s\n", output_path);
}

static void write_csv(
    const char *output_directory,
    uint32_t timestep_ms,
    int32_t scale)
{
    char filename[64];
    char output_path[512];
    (void)snprintf(filename, sizeof(filename),
        "egm_lut_%ums.csv", timestep_ms);
    FILE *output = open_output(
        output_directory, filename, output_path, sizeof(output_path));

    fprintf(output,
        "timestep_ms,direction,progress_step,elapsed_ms,"
        "dipole_x_mm,dipole_y_mm,reference_potential,integer_value,"
        "reconstructed_potential,error,absolute_error\n");

    const uint32_t step_count = EGM_PATH_DELAY_MS / timestep_ms;
    for (uint32_t direction = 0U;
         direction < EGM_DIRECTION_COUNT;
         ++direction) {
        for (uint32_t step = 0U; step < step_count; ++step) {
            const uint32_t elapsed_ms = step * timestep_ms;
            const EgmReferenceSample sample = reference_sample(
                direction, elapsed_ms);
            const int32_t integer_value = quantize(sample.potential, scale);
            const double reconstructed =
                (double)integer_value / (double)scale;
            const double error =
                reconstructed - (double)sample.potential;

            fprintf(output,
                "%u,%s,%u,%u,%.9g,%.9g,%.9g,%d,%.12g,%.12g,%.12g\n",
                timestep_ms,
                direction == EGM_DIRECTION_A_TO_B ? "A_TO_B" : "B_TO_A",
                step, elapsed_ms,
                sample.dipole_x_mm, sample.dipole_y_mm,
                sample.potential, integer_value, reconstructed,
                error, fabs(error));
        }
    }

    fclose(output);
    printf("generated %s\n", output_path);
}

static void write_report(
    const char *output_directory,
    int32_t scale,
    float maximum_absolute_reference,
    const EgmTimestepStatistics *statistics,
    size_t statistics_count)
{
    char output_path[512];
    FILE *output = open_output(
        output_directory,
        "EGM_LUT_GENERATION_REPORT.md",
        output_path,
        sizeof(output_path));
    const double maximum_single_integer =
        (double)maximum_absolute_reference * (double)scale;
    const double maximum_accumulator_bound =
        maximum_single_integer * (double)EGM_MAX_SIMULTANEOUS_PATHS;

    fprintf(output,
        "# One-Path EGM Lookup Generation Report\n\n"
        "Generated by `tools/generate_egm_lut.c`.\n\n"
        "## Reference configuration\n\n"
        "- Path: A `(0, 0)` to B `(%u, 0)` mm\n"
        "- Path delay: %u ms\n"
        "- Electrode: row %u, column %u, height %u mm\n"
        "- Dipole moment: %.9g\n"
        "- Longitudinal weight: %.9g\n"
        "- Transverse weight: %.9g\n"
        "- Maximum simultaneous paths reserved in range analysis: %u\n\n"
        "## Integer scale and range\n\n"
        "- Common scale: `%d` integer units per model potential unit\n"
        "- Maximum absolute floating reference: `%.12g`\n"
        "- Conservative four-path accumulator magnitude: `%.0f`\n"
        "- Signed 32-bit limit: `%d`\n"
        "- Scale-selection headroom: `%.0f%%` of the signed 32-bit limit\n\n"
        "The same scale is used for all timesteps so their integer outputs are "
        "directly comparable.\n\n"
        "## Per-timestep results\n\n"
        "| Timestep | Steps/direction | Samples | Minimum | Maximum | "
        "Maximum absolute quantization error | RMSE |\n"
        "|---:|---:|---:|---:|---:|---:|---:|\n",
        EGM_PATH_GAP_MM, EGM_PATH_DELAY_MS,
        EGM_ELECTRODE_ROW, EGM_ELECTRODE_COL,
        EGM_ELECTRODE_HEIGHT_MM,
        EGM_DIPOLE_MOMENT, EGM_LONGITUDINAL_WEIGHT,
        EGM_TRANSVERSE_WEIGHT, EGM_MAX_SIMULTANEOUS_PATHS,
        scale, maximum_absolute_reference,
        maximum_accumulator_bound, INT32_MAX,
        EGM_SCALE_HEADROOM * 100.0);

    for (size_t index = 0U; index < statistics_count; ++index) {
        const EgmTimestepStatistics *entry = &statistics[index];
        fprintf(output,
            "| %u ms | %u | %u | %.9g | %.9g | %.12g | %.12g |\n",
            entry->timestep_ms,
            entry->step_count,
            entry->step_count * EGM_DIRECTION_COUNT,
            entry->minimum_potential,
            entry->maximum_potential,
            entry->maximum_absolute_error,
            entry->rmse);
    }

    fprintf(output,
        "\n## Interpretation\n\n"
        "The tables contain every propagation state reachable before relay "
        "for the selected path delay. The 10 ms table contains more temporal "
        "samples than the 200 ms table, but all tables represent the same "
        "continuous moving-dipole equation at their respective sample times.\n"
        "The reported error is only the conversion from the single-precision "
        "reference potential to the common signed-integer scale.\n");

    fclose(output);
    printf("generated %s\n", output_path);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (EGM_ELECTRODE_HEIGHT_MM == 0U) {
        fprintf(stderr,
            "electrode height must be nonzero for this reference geometry\n");
        return EXIT_FAILURE;
    }

    const float maximum_absolute_reference =
        find_maximum_absolute_reference();
    const int32_t scale = choose_common_decimal_scale(
        maximum_absolute_reference);
    EgmTimestepStatistics statistics[
        sizeof(kSupportedTimestepsMs) / sizeof(kSupportedTimestepsMs[0])];

    for (size_t index = 0U;
         index < sizeof(kSupportedTimestepsMs) /
             sizeof(kSupportedTimestepsMs[0]);
         ++index) {
        const uint32_t timestep_ms = kSupportedTimestepsMs[index];
        write_header(argv[1], timestep_ms, scale, &statistics[index]);
        write_csv(argv[1], timestep_ms, scale);
    }

    write_report(
        argv[1],
        scale,
        maximum_absolute_reference,
        statistics,
        sizeof(statistics) / sizeof(statistics[0]));

    return EXIT_SUCCESS;
}
