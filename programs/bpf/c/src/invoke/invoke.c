/**
 * @brief Example C-based BPF program that tests cross-program invocations
 */
#include "../invoked/instruction.h"
#include <solana_sdk.h>

static const uint8_t TEST_SUCCESS = 1;
static const uint8_t TEST_PRIVILEGE_ESCALATION_SIGNER = 2;
static const uint8_t TEST_PRIVILEGE_ESCALATION_WRITABLE = 3;
static const uint8_t TEST_PPROGRAM_NOT_EXECUTABLE = 4;
static const uint8_t TEST_EMPTY_ACCOUNTS_SLICE = 5;
static const uint8_t TEST_CAP_SEEDS = 6;
static const uint8_t TEST_CAP_SIGNERS = 7;
static const uint8_t TEST_ALLOC_ACCESS_VIOLATION = 8;
static const uint8_t TEST_INSTRUCTION_DATA_TOO_LARGE = 9;
static const uint8_t TEST_INSTRUCTION_META_TOO_LARGE = 10;
static const uint8_t TEST_RETURN_ERROR = 11;
static const uint8_t TEST_PRIVILEGE_DEESCALATION_ESCALATION_SIGNER = 12;
static const uint8_t TEST_PRIVILEGE_DEESCALATION_ESCALATION_WRITABLE = 13;
static const uint8_t TEST_WRITABLE_DEESCALATION_WRITABLE = 14;

static const int MINT_INDEX = 0;
static const int ARGUMENT_INDEX = 1;
static const int INVOKED_PROGRAM_INDEX = 2;
static const int INVOKED_ARGUMENT_INDEX = 3;
static const int INVOKED_PROGRAM_DUP_INDEX = 4;
static const int ARGUMENT_DUP_INDEX = 5;
static const int DERIVED_KEY1_INDEX = 6;
static const int DERIVED_KEY2_INDEX = 7;
static const int DERIVED_KEY3_INDEX = 8;
static const int SYSTEM_PROGRAM_INDEX = 9;
static const int FROM_INDEX = 10;

extern uint64_t entrypoint(const uint8_t *input) {
  sol_log("Invoke C program");

  SolAccountInfo accounts[11];
  SolParameters params = (SolParameters){.ka = accounts};

  if (!sol_deserialize(input, &params, SOL_ARRAY_SIZE(accounts))) {
    return ERROR_INVALID_ARGUMENT;
  }

  uint8_t bump_seed1 = params.data[1];
  uint8_t bump_seed2 = params.data[2];
  uint8_t bump_seed3 = params.data[3];

  switch (params.data[0]) {
  case TEST_SUCCESS: {
    sol_log("Call system program create account");
    {
      uint64_t from_lamports = *accounts[FROM_INDEX].lamports;
      uint64_t to_lamports = *accounts[DERIVED_KEY1_INDEX].lamports;
      SolAccountMeta arguments[] = {
          {accounts[FROM_INDEX].key, true, true},
          {accounts[DERIVED_KEY1_INDEX].key, true, true}};
      uint8_t data[4 + 8 + 8 + 32];
      *(uint64_t *)(data + 4) = 42;
      *(uint64_t *)(data + 4 + 8) = MAX_PERMITTED_DATA_INCREASE;
      sol_memcpy(data + 4 + 8 + 8, params.program_id, SIZE_PUBKEY);
      const SolInstruction instruction = {accounts[SYSTEM_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      uint8_t seed1[] = {'Y', 'o', 'u', ' ', 'p', 'a', 's', 's',
                         ' ', 'b', 'u', 't', 't', 'e', 'r'};
      const SolSignerSeed seeds1[] = {{seed1, SOL_ARRAY_SIZE(seed1)},
                                      {&bump_seed1, 1}};
      const SolSignerSeeds signers_seeds[] = {{seeds1, SOL_ARRAY_SIZE(seeds1)}};
      sol_assert(SUCCESS == sol_invoke_signed(&instruction, accounts,
                                              SOL_ARRAY_SIZE(accounts),
                                              signers_seeds,
                                              SOL_ARRAY_SIZE(signers_seeds)));
      sol_assert(*accounts[FROM_INDEX].lamports == from_lamports - 42);
      sol_assert(*accounts[DERIVED_KEY1_INDEX].lamports == to_lamports + 42);
      sol_assert(SolPubkey_same(accounts[DERIVED_KEY1_INDEX].owner,
                                params.program_id));
      sol_assert(accounts[DERIVED_KEY1_INDEX].data_len ==
                 MAX_PERMITTED_DATA_INCREASE);
      sol_assert(
          accounts[DERIVED_KEY1_INDEX].data[MAX_PERMITTED_DATA_INCREASE - 1] ==
          0);
      accounts[DERIVED_KEY1_INDEX].data[MAX_PERMITTED_DATA_INCREASE - 1] = 0x0f;
      sol_assert(
          accounts[DERIVED_KEY1_INDEX].data[MAX_PERMITTED_DATA_INCREASE - 1] ==
          0x0f);
      for (uint8_t i = 0; i < 20; i++) {
        accounts[DERIVED_KEY1_INDEX].data[i] = i;
      }
    }

    sol_log("Call system program transfer");
    {
      uint64_t from_lamports = *accounts[FROM_INDEX].lamports;
      uint64_t to_lamports = *accounts[DERIVED_KEY1_INDEX].lamports;
      SolAccountMeta arguments[] = {
          {accounts[FROM_INDEX].key, true, true},
          {accounts[DERIVED_KEY1_INDEX].key, true, false}};
      uint8_t data[] = {2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
      const SolInstruction instruction = {accounts[SYSTEM_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
      sol_assert(*accounts[FROM_INDEX].lamports == from_lamports - 1);
      sol_assert(*accounts[DERIVED_KEY1_INDEX].lamports == to_lamports + 1);
    }

    sol_log("Test data translation");
    {
      for (int i = 0; i < accounts[ARGUMENT_INDEX].data_len; i++) {
        accounts[ARGUMENT_INDEX].data[i] = i;
      }

      SolAccountMeta arguments[] = {
          {accounts[ARGUMENT_INDEX].key, true, true},
          {accounts[INVOKED_ARGUMENT_INDEX].key, true, true},
          {accounts[INVOKED_PROGRAM_INDEX].key, false, false},
          {accounts[INVOKED_PROGRAM_DUP_INDEX].key, false, false}};
      uint8_t data[] = {VERIFY_TRANSLATIONS, 1, 2, 3, 4, 5};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};

      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    }

    sol_log("Test no instruction data");
    {
      SolAccountMeta arguments[] = {{accounts[ARGUMENT_INDEX].key, true, true}};
      uint8_t data[] = {};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};

      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    }

    sol_log("Test create_program_address");
    {
      uint8_t seed1[] = {'Y', 'o', 'u', ' ', 'p', 'a', 's', 's',
                         ' ', 'b', 'u', 't', 't', 'e', 'r'};
      const SolSignerSeed seeds1[] = {{seed1, SOL_ARRAY_SIZE(seed1)},
                                      {&bump_seed1, 1}};
      SolPubkey address;
      sol_assert(SUCCESS ==
                 sol_create_program_address(seeds1, SOL_ARRAY_SIZE(seeds1),
                                            params.program_id, &address));
      sol_assert(SolPubkey_same(&address, accounts[DERIVED_KEY1_INDEX].key));
    }

    sol_log("Test try_find_program_address");
    {
      uint8_t seed[] = {'Y', 'o', 'u', ' ', 'p', 'a', 's', 's',
                        ' ', 'b', 'u', 't', 't', 'e', 'r'};
      const SolSignerSeed seeds[] = {{seed, SOL_ARRAY_SIZE(seed)}};
      SolPubkey address;
      uint8_t bump_seed;
      sol_assert(SUCCESS == sol_try_find_program_address(
                                seeds, SOL_ARRAY_SIZE(seeds), params.program_id,
                                &address, &bump_seed));
      sol_assert(SolPubkey_same(&address, accounts[DERIVED_KEY1_INDEX].key));
      sol_assert(bump_seed == bump_seed1);
    }

    sol_log("Test derived signers");
    {
      sol_assert(!accounts[DERIVED_KEY1_INDEX].is_signer);
      sol_assert(!accounts[DERIVED_KEY2_INDEX].is_signer);
      sol_assert(!accounts[DERIVED_KEY3_INDEX].is_signer);

      SolAccountMeta arguments[] = {
          {accounts[INVOKED_PROGRAM_INDEX].key, false, false},
          {accounts[DERIVED_KEY1_INDEX].key, true, true},
          {accounts[DERIVED_KEY2_INDEX].key, true, false},
          {accounts[DERIVED_KEY3_INDEX].key, false, false}};
      uint8_t data[] = {DERIVED_SIGNERS, bump_seed2, bump_seed3};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      uint8_t seed1[] = {'Y', 'o', 'u', ' ', 'p', 'a', 's', 's',
                         ' ', 'b', 'u', 't', 't', 'e', 'r'};
      const SolSignerSeed seeds1[] = {{seed1, SOL_ARRAY_SIZE(seed1)},
                                      {&bump_seed1, 1}};
      const SolSignerSeeds signers_seeds[] = {{seeds1, SOL_ARRAY_SIZE(seeds1)}};
      sol_assert(SUCCESS == sol_invoke_signed(&instruction, accounts,
                                              SOL_ARRAY_SIZE(accounts),
                                              signers_seeds,
                                              SOL_ARRAY_SIZE(signers_seeds)));
    }

    sol_log("Test readonly with writable account");
    {
      SolAccountMeta arguments[] = {
          {accounts[INVOKED_ARGUMENT_INDEX].key, true, false}};
      uint8_t data[] = {VERIFY_WRITER};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};

      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    }

    sol_log("Test invoke");
    {
      sol_assert(accounts[ARGUMENT_INDEX].is_signer);

      *accounts[ARGUMENT_INDEX].lamports -= 5;
      *accounts[INVOKED_ARGUMENT_INDEX].lamports += 5;

      SolAccountMeta arguments[] = {
          {accounts[INVOKED_ARGUMENT_INDEX].key, true, true},
          {accounts[ARGUMENT_INDEX].key, true, true},
          {accounts[INVOKED_PROGRAM_DUP_INDEX].key, false, false}};
      uint8_t data[] = {NESTED_INVOKE};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};

      sol_log("First invoke");
      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
      sol_log("2nd invoke from first program");
      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));

      sol_assert(*accounts[ARGUMENT_INDEX].lamports == 42 - 5 + 1 + 1 + 1 + 1);
      sol_assert(*accounts[INVOKED_ARGUMENT_INDEX].lamports ==
                 10 + 5 - 1 - 1 - 1 - 1);
    }

    sol_log("Test privilege deescalation");
    {
      sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_signer);
      sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_writable);
      SolAccountMeta arguments[] = {
          {accounts[INVOKED_ARGUMENT_INDEX].key, false, false}};
      uint8_t data[] = {VERIFY_PRIVILEGE_DEESCALATION};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    }

    sol_log("Verify data values are retained and updated");
    for (int i = 0; i < accounts[ARGUMENT_INDEX].data_len; i++) {
      sol_assert(accounts[ARGUMENT_INDEX].data[i] == i);
    }
    for (int i = 0; i < accounts[INVOKED_ARGUMENT_INDEX].data_len; i++) {
      sol_assert(accounts[INVOKED_ARGUMENT_INDEX].data[i] == i);
    }

    sol_log("Verify data write before ro cpi call");
    {
      for (int i = 0; i < accounts[ARGUMENT_INDEX].data_len; i++) {
        accounts[ARGUMENT_INDEX].data[i] = 0;
      }

      SolAccountMeta arguments[] = {
          {accounts[ARGUMENT_INDEX].key, false, false}};
      uint8_t data[] = {VERIFY_PRIVILEGE_DEESCALATION};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      sol_assert(SUCCESS ==
                 sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));

      for (int i = 0; i < accounts[ARGUMENT_INDEX].data_len; i++) {
        sol_assert(accounts[ARGUMENT_INDEX].data[i] == 0);
      }
    }
    break;
  }
  case TEST_PRIVILEGE_ESCALATION_SIGNER: {
    sol_log("Test privilege escalation signer");
    SolAccountMeta arguments[] = {
        {accounts[DERIVED_KEY3_INDEX].key, false, false}};
    uint8_t data[] = {VERIFY_PRIVILEGE_ESCALATION};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    sol_assert(SUCCESS ==
               sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));

    // Signer privilege escalation will always fail the whole transaction
    instruction.accounts[0].is_signer = true;
    sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts));
    break;
  }
  case TEST_PRIVILEGE_ESCALATION_WRITABLE: {
    sol_log("Test privilege escalation writable");
    SolAccountMeta arguments[] = {
        {accounts[DERIVED_KEY3_INDEX].key, false, false}};
    uint8_t data[] = {VERIFY_PRIVILEGE_ESCALATION};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    sol_assert(SUCCESS ==
               sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));

    // Writable privilege escalation will always fail the whole transaction
    instruction.accounts[0].is_writable = true;
    sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts));
    break;
  }
  case TEST_PPROGRAM_NOT_EXECUTABLE: {
    sol_log("Test program not executable");
    SolAccountMeta arguments[] = {
        {accounts[DERIVED_KEY3_INDEX].key, false, false}};
    uint8_t data[] = {VERIFY_PRIVILEGE_ESCALATION};
    const SolInstruction instruction = {accounts[ARGUMENT_INDEX].key, arguments,
                                        SOL_ARRAY_SIZE(arguments), data,
                                        SOL_ARRAY_SIZE(data)};
    return sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts));
  }
  case TEST_EMPTY_ACCOUNTS_SLICE: {
    sol_log("Empty accounts slice");

    SolAccountMeta arguments[] = {
        {accounts[INVOKED_ARGUMENT_INDEX].key, false, false}};
    uint8_t data[] = {};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};

    sol_assert(SUCCESS == sol_invoke(&instruction, 0, 0));
    break;
  }
  case TEST_CAP_SEEDS: {
    sol_log("Test cap seeds");
    SolAccountMeta arguments[] = {};
    uint8_t data[] = {};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    uint8_t seed[] = {"seed"};
    const SolSignerSeed seeds[] = {
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)}, {seed, SOL_ARRAY_SIZE(seed)},
        {seed, SOL_ARRAY_SIZE(seed)},
    };
    const SolSignerSeeds signers_seeds[] = {{seeds, SOL_ARRAY_SIZE(seeds)}};
    sol_assert(SUCCESS == sol_invoke_signed(
                              &instruction, accounts, SOL_ARRAY_SIZE(accounts),
                              signers_seeds, SOL_ARRAY_SIZE(signers_seeds)));
    break;
  }
  case TEST_CAP_SIGNERS: {
    sol_log("Test cap signers");
    SolAccountMeta arguments[] = {};
    uint8_t data[] = {};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    uint8_t seed[] = {"seed"};
    const SolSignerSeed seed1[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed2[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed3[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed4[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed5[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed6[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed7[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed8[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed9[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed10[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed11[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed12[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed13[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed14[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed15[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed16[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeed seed17[] = {{seed, SOL_ARRAY_SIZE(seed)}};
    const SolSignerSeeds signers_seeds[] = {
        {seed1, SOL_ARRAY_SIZE(seed1)},   {seed2, SOL_ARRAY_SIZE(seed2)},
        {seed3, SOL_ARRAY_SIZE(seed3)},   {seed4, SOL_ARRAY_SIZE(seed4)},
        {seed5, SOL_ARRAY_SIZE(seed5)},   {seed6, SOL_ARRAY_SIZE(seed6)},
        {seed7, SOL_ARRAY_SIZE(seed7)},   {seed8, SOL_ARRAY_SIZE(seed8)},
        {seed9, SOL_ARRAY_SIZE(seed9)},   {seed10, SOL_ARRAY_SIZE(seed10)},
        {seed11, SOL_ARRAY_SIZE(seed11)}, {seed12, SOL_ARRAY_SIZE(seed12)},
        {seed13, SOL_ARRAY_SIZE(seed13)}, {seed14, SOL_ARRAY_SIZE(seed14)},
        {seed15, SOL_ARRAY_SIZE(seed15)}, {seed16, SOL_ARRAY_SIZE(seed16)},
        {seed17, SOL_ARRAY_SIZE(seed17)}};
    sol_assert(SUCCESS == sol_invoke_signed(
                              &instruction, accounts, SOL_ARRAY_SIZE(accounts),
                              signers_seeds, SOL_ARRAY_SIZE(signers_seeds)));
    break;
  }
  case TEST_ALLOC_ACCESS_VIOLATION: {
    sol_log("Test resize violation");
    SolAccountMeta arguments[] = {
        {accounts[FROM_INDEX].key, true, true},
        {accounts[DERIVED_KEY1_INDEX].key, true, true}};
    uint8_t data[4 + 8 + 8 + 32];
    *(uint64_t *)(data + 4) = 42;
    *(uint64_t *)(data + 4 + 8) = MAX_PERMITTED_DATA_INCREASE;
    sol_memcpy(data + 4 + 8 + 8, params.program_id, SIZE_PUBKEY);
    const SolInstruction instruction = {accounts[SYSTEM_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    uint8_t seed1[] = {'Y', 'o', 'u', ' ', 'p', 'a', 's', 's',
                       ' ', 'b', 'u', 't', 't', 'e', 'r'};
    const SolSignerSeed seeds1[] = {{seed1, SOL_ARRAY_SIZE(seed1)},
                                    {&bump_seed1, 1}};
    const SolSignerSeeds signers_seeds[] = {{seeds1, SOL_ARRAY_SIZE(seeds1)}};

    SolAccountInfo derived_account = {
        .key = accounts[DERIVED_KEY1_INDEX].key,
        .lamports = accounts[DERIVED_KEY1_INDEX].lamports,
        .data_len = accounts[DERIVED_KEY1_INDEX].data_len,
        // Point to top edge of heap, attempt to allocate into unprivileged
        // memory
        .data = (uint8_t *)0x300007ff8,
        .owner = accounts[DERIVED_KEY1_INDEX].owner,
        .rent_epoch = accounts[DERIVED_KEY1_INDEX].rent_epoch,
        .is_signer = accounts[DERIVED_KEY1_INDEX].is_signer,
        .is_writable = accounts[DERIVED_KEY1_INDEX].is_writable,
        .executable = accounts[DERIVED_KEY1_INDEX].executable,
    };
    const SolAccountInfo invoke_accounts[] = {
        accounts[FROM_INDEX], accounts[SYSTEM_PROGRAM_INDEX], derived_account};
    sol_assert(SUCCESS ==
               sol_invoke_signed(&instruction,
                                 (const SolAccountInfo *)invoke_accounts, 3,
                                 signers_seeds, SOL_ARRAY_SIZE(signers_seeds)));
    break;
  }
  case TEST_INSTRUCTION_DATA_TOO_LARGE: {
    sol_log("Test instruction data too large");
    SolAccountMeta arguments[] = {};
    uint8_t *data = sol_calloc(1500, 1);
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, 1500};
    const SolSignerSeeds signers_seeds[] = {};
    sol_assert(SUCCESS == sol_invoke_signed(
                              &instruction, accounts, SOL_ARRAY_SIZE(accounts),
                              signers_seeds, SOL_ARRAY_SIZE(signers_seeds)));

    break;
  }
  case TEST_INSTRUCTION_META_TOO_LARGE: {
    sol_log("Test instruction meta too large");
    SolAccountMeta *arguments = sol_calloc(40, sizeof(SolAccountMeta));
    sol_log_64(0, 0, 0, 0, (uint64_t)arguments);
    sol_assert(0 != arguments);
    uint8_t data[] = {};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, 40, data,
                                        SOL_ARRAY_SIZE(data)};
    const SolSignerSeeds signers_seeds[] = {};
    sol_assert(SUCCESS == sol_invoke_signed(
                              &instruction, accounts, SOL_ARRAY_SIZE(accounts),
                              signers_seeds, SOL_ARRAY_SIZE(signers_seeds)));

    break;
  }
  case TEST_RETURN_ERROR: {
    sol_log("Test return error");
    SolAccountMeta arguments[] = {{accounts[ARGUMENT_INDEX].key, false, true}};
    uint8_t data[] = {RETURN_ERROR};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};

    sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts));
    break;
  }
  case TEST_PRIVILEGE_DEESCALATION_ESCALATION_SIGNER: {
    sol_log("Test privilege deescalation escalation signer");
    sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_signer);
    sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_writable);
    SolAccountMeta arguments[] = {
        {accounts[INVOKED_PROGRAM_INDEX].key, false, false},
        {accounts[INVOKED_ARGUMENT_INDEX].key, false, false}};
    uint8_t data[] = {VERIFY_PRIVILEGE_DEESCALATION_ESCALATION_SIGNER};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    sol_assert(SUCCESS ==
               sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    break;
  }
  case TEST_PRIVILEGE_DEESCALATION_ESCALATION_WRITABLE: {
    sol_log("Test privilege deescalation escalation writable");
    sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_signer);
    sol_assert(true == accounts[INVOKED_ARGUMENT_INDEX].is_writable);
    SolAccountMeta arguments[] = {
        {accounts[INVOKED_PROGRAM_INDEX].key, false, false},
        {accounts[INVOKED_ARGUMENT_INDEX].key, false, false}};
    uint8_t data[] = {VERIFY_PRIVILEGE_DEESCALATION_ESCALATION_WRITABLE};
    const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                        arguments, SOL_ARRAY_SIZE(arguments),
                                        data, SOL_ARRAY_SIZE(data)};
    sol_assert(SUCCESS ==
               sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts)));
    break;
  }
  case TEST_WRITABLE_DEESCALATION_WRITABLE: {
  sol_log("Test writable deescalation");
      uint8_t buffer[10];
      for (int i = 0; i < 10; i++) {
        buffer[i] = accounts[INVOKED_ARGUMENT_INDEX].data[i];
      }
      SolAccountMeta arguments[] = {
          {accounts[INVOKED_ARGUMENT_INDEX].key, false, false}};
      uint8_t data[] = {WRITE_ACCOUNT, 10};
      const SolInstruction instruction = {accounts[INVOKED_PROGRAM_INDEX].key,
                                          arguments, SOL_ARRAY_SIZE(arguments),
                                          data, SOL_ARRAY_SIZE(data)};
      sol_invoke(&instruction, accounts, SOL_ARRAY_SIZE(accounts));

      for (int i = 0; i < 10; i++) {
        sol_assert(buffer[i] == accounts[INVOKED_ARGUMENT_INDEX].data[i]);
      }
      break;
    }
  default:
    sol_panic();
  }

  return SUCCESS;
}
