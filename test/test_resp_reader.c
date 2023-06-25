#include <string.h>
#include "test_common.h"

#include "../src/ext/readerResp.c"

#define STR_AND_SIZE(str) str, (sizeof(str)-1)

static void test_resp_reader_common(RespReaderCtx *ctx,
                                    char *payload,
                                    int payloadSize,
                                    int initCtx,
                                    int expRes,
                                    int expReplies)
{
    if (initCtx) readRespInit(ctx);

    RespRes res = readRespReplies(ctx, payload, payloadSize);
    assert_int_equal(res, expRes);
    assert_int_equal(ctx->countReplies, expReplies);
}

static void test_single_status(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n"),
                            1, RESP_REPLY_OK, 1);
}

static void test_single_int(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE(":1\r\n"),
                            1, RESP_REPLY_OK, 1);
}

static void test_three_statuses(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n+OK\r\n+OK\r\n"),
                            1, RESP_REPLY_OK, 3);
}

static void test_two_statuses_and_partial_reply(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n+OK\r\n+OK\r"),
                            1, RESP_REPLY_PARTIAL, 2);
}

static void test_reply_fragmented(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r"),
                            1, RESP_REPLY_PARTIAL, 0);
    test_resp_reader_common(&ctx, STR_AND_SIZE("\n"),
                            0, RESP_REPLY_OK, 1);
}

static void test_reply_error(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n-ERR This is an error1\r\n+OK\r\n"),
                            1, RESP_REPLY_ERR, 1);
    assert_string_equal(ctx.errorMsg, "ERR This is an error1");

    test_resp_reader_common(&ctx,
                            STR_AND_SIZE("+OK\r\n-ERR This is an error2\r\n-Any data afterward won't get processed"),
                            1, RESP_REPLY_ERR, 1);
    assert_string_equal(ctx.errorMsg, "ERR This is an error2");
}

static void test_reply_error_fragmented(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n-ERR This "), 1, RESP_REPLY_PARTIAL, 1);
    test_resp_reader_common(&ctx, STR_AND_SIZE("is an "), 0, RESP_REPLY_PARTIAL, 1);
    test_resp_reader_common(&ctx, STR_AND_SIZE("error message\r"), 0, RESP_REPLY_PARTIAL, 1);
    test_resp_reader_common(&ctx, STR_AND_SIZE("\n"), 0, RESP_REPLY_ERR, 1);
    assert_string_equal(ctx.errorMsg, "ERR This is an error message");
}


static void test_reply_long_err_trimmed_by_report(void **state) {
    UNUSED(state);
    RespReaderCtx ctx;
    int errMsgLen = MAX_RESP_REPLY_ERR_MSG + 7; /* overflow */
    char errMsg[MAX_RESP_REPLY_ERR_MSG + 7];
    const char errorChunk[] = "0123456789ABCDEF";

    /* build error message */
    for (int i = 0 ; i < errMsgLen ; ++i)
        errMsg[i] = errorChunk[i%16];

    test_resp_reader_common(&ctx, STR_AND_SIZE("+OK\r\n-"), 1, RESP_REPLY_PARTIAL, 1);
    test_resp_reader_common(&ctx, errMsg, errMsgLen, 0, RESP_REPLY_PARTIAL, 1);
    test_resp_reader_common(&ctx, STR_AND_SIZE("\r\n-"), 0, RESP_REPLY_ERR, 1);
    errMsg[MAX_RESP_REPLY_ERR_MSG-1] = '\0'; /* reported error is trimmed */
    assert_string_equal(ctx.errorMsg, errMsg);

}

/*************************** group_test_resp_reader *******************************/
int group_test_resp_reader(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_single_status),
            cmocka_unit_test(test_single_int),
            cmocka_unit_test(test_three_statuses),
            cmocka_unit_test(test_two_statuses_and_partial_reply),
            cmocka_unit_test(test_reply_fragmented),
            cmocka_unit_test(test_reply_error),
            cmocka_unit_test(test_reply_long_err_trimmed_by_report),
            cmocka_unit_test(test_reply_error_fragmented),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}