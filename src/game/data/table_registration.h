#pragma once

// Registration of the table entry classes (by name, for Table loading) and of the SD record factories (by
// record family range). Each Ensure* wrapper registers once.
void RegisterTableEntryClasses();
int EnsureTableEntryClassesRegistered();
void RegisterRecordFactories();
int EnsureRecordFactoriesRegistered();
