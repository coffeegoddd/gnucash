# Research: How GnuCash Ingests Raw Bank Data

**Issue**: gn-97d
**Date**: 2026-03-13
**Scope**: Data acquisition and storage only — NOT matching, categorization, or ledger posting.

## Summary

GnuCash supports five import sources, all funneling through a generic import
backend (`gnucash/import-export/import-backend.cpp`). Each source-specific
importer parses its format and creates a GnuCash Transaction with a single
unbalanced Split (the "source split"). The generic importer then handles
duplicate detection, matching, and completion.

There is **no intermediate staging data structure** between the raw parsed data
and GnuCash Transaction objects. The CSV importer's `DraftTransaction` is the
closest analog but is immediately converted.

---

## 1. Supported Formats

| Source | Library/Parser | Protocol/Format | Entry Point |
|--------|---------------|-----------------|-------------|
| OFX/QFX | libofx (C) | OFX 1.x SGML, OFX 2.x XML | `ofx/gnc-ofx-import.cpp` → `ofx_proc_transaction_cb()` |
| QIF | Scheme scripts | Quicken Interchange Format (text) | `qif-imp/qif-file.scm` → `qif-to-gnc.scm` |
| CSV/Fixed-Width | C++ tokenizers | User-mapped columns | `csv-imp/gnc-imp-props-tx.cpp` → `GncPreTrans`/`GncPreSplit` |
| AQBanking | AQBanking + Gwenhywfar | HBCI, FinTS, SWIFT MT940/MT942, DTAUS | `aqb/gnc-ab-utils.c` → `gnc_ab_trans_to_gnc()` |
| Log Replay | C++ | GnuCash internal log format | `log-replay/gnc-log-replay.cpp` (not a bank source) |

## 2. Fields Per Source

### OFX (from libofx's OfxTransactionData)

**Required:**
- `amount` (double) — transaction amount
- `account_id` (string) — maps to GnuCash account via online_id

**Common:**
- `date_posted`, `date_initiated`, `date_funds_available` (time_t)
- `fi_id` / FITID (string) — unique transaction ID for dedup
- `name` (string) → Description
- `memo` (string) → Split memo (or Description if name absent)
- `check_number`, `reference_number`
- `transactiontype` (enum: CREDIT, DEBIT, INT, DIV, FEE, SRVCHG, DEP, ATM, POS, XFER, CHECK, PAYMENT, CASH, DIRECTDEP, DIRECTDEBIT, REPEATPMT, OTHER)
- `server_transaction_id` (confirmation number)
- `standard_industrial_code` (SIC)
- `payee_id`

**Investment-specific:**
- `invtransactiontype` (BUYSTOCK, SELLSTOCK, INCOME, REINVEST, etc.)
- `unique_id` (CUSIP/ISIN), `units`, `unitprice`
- `security_data_ptr` → secname, ticker, memo

### QIF

**Non-investment:** Date, Amount (T/U), Cleared status, Check number, Payee, Memo, Address (up to 5 lines), Category/Transfer/Class, Split lines (S/E/$)

**Investment:** Date, Action (Buy/Sell/Div/etc.), Security, Price, Quantity, Commission, Transfer account, Amount

**Limitations:** No unique transaction ID, ambiguous date/number formats, no currency info.

### CSV/Fixed-Width (GncTransPropType enum)

Fully user-configurable mapping:
- Transaction: UNIQUE_ID, DATE, NUM, DESCRIPTION, NOTES, COMMODITY
- Split: ACTION, ACCOUNT, AMOUNT, VALUE, PRICE, MEMO, REC_STATE, REC_DATE
- Transfer split: TACTION, TACCOUNT, TAMOUNT, TMEMO, TREC_STATE, TREC_DATE

### AQBanking (AB_TRANSACTION fields)

- Value (amount + currency), ValutaDate (effective), Date (posting)
- RemoteName (counterparty), TransactionText (bank description)
- PurposeAsStringList (multi-line reference)
- UltimateCreditor / UltimateDebtor (SWIFT/CAMT.053)
- RemoteAccountNumber / RemoteIban, RemoteBankCode / RemoteBic
- FiId (bank's unique ID), CustomerReference
- LocalBankCode, LocalAccountNumber

## 3. Normalized Common Schema

All importers produce a GnuCash Transaction + Split with these common fields:

### Transaction-level
| Field | Type | Source |
|-------|------|--------|
| Date posted | time64 | All sources |
| Date entered | time64 | Always current time |
| Description | string | name/payee/remote-name |
| Currency | gnc_commodity* | Account's commodity |
| Notes | string (optional) | OFX metadata, otherwise empty |
| Num | string (optional) | Check number or customer ref |

### Split-level (source split only)
| Field | Type | Source |
|-------|------|--------|
| Account | Account* | Import/source account |
| Amount/Value | gnc_numeric | Transaction amount |
| Memo | string (optional) | Varies by source |
| online_id | KVP string | FITID/FiId (for dedup) |
| Action | string (optional) | Check number or investment action |
| Reconcile state | char (optional) | QIF/CSV only |

### Import wrapper (_transactioninfo)
| Field | Type | Purpose |
|-------|------|---------|
| trans | Transaction* | The actual transaction |
| first_split | Split* | Source-side split |
| match_list | GList* | Populated by matching algorithm |
| action | GNCImportAction | SKIP, ADD, CLEAR, UPDATE |
| dest_acc | Account* | Destination for balancing split |
| ref_id | guint32 | External reference (e.g., AQBanking job ID) |
| lsplit_* | various | Data for constructing balancing split |

## 4. Modern Bank APIs (Not in GnuCash)

| API | Region | Delivery | Key Extra Fields |
|-----|--------|----------|-----------------|
| Plaid | US/CA/UK/EU | REST + webhooks | merchant_name, category[], payment_channel, location, pending, personal_finance_category, counterparties[], logo_url |
| Yodlee | US | REST | description.original/.simple/.consumer, merchant.categoryLabel, status (POSTED/PENDING) |
| MX | US | REST | original_description, merchant_category_code (MCC), latitude/longitude |
| Open Banking (PSD2) | UK/EU | REST (Berlin Group/OBIE) | creditorName, creditorAccount (IBAN), debtorName, remittanceInformationUnstructured, bankTransactionCode, merchantCategoryCode |

### Key differences from file formats:
1. Real-time/near-real-time vs batch downloads
2. Richer merchant data (MCC codes, names, logos, locations)
3. Pending transactions surfaced before posting
4. Pre-categorization built in
5. Structured counterparty info (IBAN/BIC natively)
6. Push model (webhooks) vs pull/file model

## 5. Architectural Insight

The critical observation: **there is no staging layer**. Format-specific importers
create real GnuCash Transaction objects directly. The `GNCImportTransInfo` wrapper
adds match/action metadata but the underlying data is already a committed-in-memory
Transaction.

For an agent-driven pipeline, a new staging structure would need to capture:
1. The common normalized fields (date, amount, description, memo, online_id)
2. Source-specific metadata (transaction type, counterparty details, SIC codes)
3. Source provenance (which format, which account, import timestamp)
4. Status tracking (raw → matched → categorized → posted)

The CSV importer's `DraftTransaction` / `GncPreTrans` / `GncPreSplit` pattern is
the closest existing model for such a staging layer.
