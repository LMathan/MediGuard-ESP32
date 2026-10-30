-- ============================================================================
-- MediGuard Smart Clinical Medicine Storage & Monitoring System
-- Supabase Schema: Unregistered RFID Medicines Tracking
-- ============================================================================
-- Purpose:
-- When an RFID tag is tapped on the ESP32:
-- 1. If the tag is NOT already registered in public.medicines, it is stored
--    in this public.unregistered_medicines table.
-- 2. The mobile application reads this table to alert the user:
--    "New Unregistered Tag Scanned: <UID> - Tap to register medicine".
-- 3. The ESP32 OLED displays:
--    [!] INVALID ID [!]
--    UID: <UID>
--    NOT REGISTERED!
--    Sent to Mobile App
-- ============================================================================

-- 1. Create the table
CREATE TABLE IF NOT EXISTS public.unregistered_medicines (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    rfid_uid TEXT NOT NULL UNIQUE,
    compartment TEXT CHECK (compartment IN ('normal', 'cold', 'unknown')) DEFAULT 'unknown',
    device_id TEXT DEFAULT 'mediguard-container-01',
    status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'registered', 'ignored')),
    scan_count INTEGER NOT NULL DEFAULT 1,
    first_scanned_at TIMESTAMPTZ NOT NULL DEFAULT timezone('utc'::text, now()),
    last_scanned_at TIMESTAMPTZ NOT NULL DEFAULT timezone('utc'::text, now()),
    metadata JSONB DEFAULT '{}'::jsonb
);

-- 2. Create search indexes
CREATE INDEX IF NOT EXISTS idx_unregistered_medicines_uid 
    ON public.unregistered_medicines(rfid_uid);

CREATE INDEX IF NOT EXISTS idx_unregistered_medicines_status 
    ON public.unregistered_medicines(status);

CREATE INDEX IF NOT EXISTS idx_unregistered_medicines_scanned 
    ON public.unregistered_medicines(last_scanned_at DESC);

-- 3. Enable Row Level Security (RLS)
ALTER TABLE public.unregistered_medicines ENABLE ROW LEVEL SECURITY;

-- 4. Policies for Authenticated Mobile App Users
CREATE POLICY "Allow authenticated read on unregistered_medicines"
ON public.unregistered_medicines
FOR SELECT
TO authenticated
USING (true);

CREATE POLICY "Allow authenticated update on unregistered_medicines"
ON public.unregistered_medicines
FOR UPDATE
TO authenticated
USING (true)
WITH CHECK (true);

CREATE POLICY "Allow authenticated delete on unregistered_medicines"
ON public.unregistered_medicines
FOR DELETE
TO authenticated
USING (true);

-- 5. Policies for ESP32 Hardware & Service Role
CREATE POLICY "Allow service_role and anon insert/upsert"
ON public.unregistered_medicines
FOR ALL
TO anon, service_role
USING (true)
WITH CHECK (true);

-- 6. Helper Function: Upsert scanned RFID
-- Can be called via RPC or Edge Function
CREATE OR REPLACE FUNCTION public.handle_rfid_scan(
    p_rfid_uid TEXT,
    p_compartment TEXT,
    p_device_id TEXT
)
RETURNS JSONB
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
DECLARE
    v_med RECORD;
    v_result JSONB;
BEGIN
    -- Check if medicine is already registered
    -- (Assumes public.medicines table has rfid_uid, name, and compartment)
    BEGIN
        SELECT name, compartment INTO v_med 
        FROM public.medicines 
        WHERE rfid_uid = UPPER(p_rfid_uid)
        LIMIT 1;
    EXCEPTION WHEN undefined_table THEN
        v_med := NULL;
    END;

    IF v_med IS NOT NULL AND v_med.name IS NOT NULL THEN
        -- Tag is registered!
        v_result := jsonb_build_object(
            'status', 'registered',
            'registered', true,
            'medicine_name', v_med.name,
            'compartment', COALESCE(v_med.compartment, p_compartment),
            'rfid_uid', UPPER(p_rfid_uid)
        );
    ELSE
        -- Tag is NOT registered: Save to unregistered_medicines
        INSERT INTO public.unregistered_medicines (
            rfid_uid,
            compartment,
            device_id,
            status,
            scan_count,
            first_scanned_at,
            last_scanned_at
        )
        VALUES (
            UPPER(p_rfid_uid),
            p_compartment,
            p_device_id,
            'pending',
            1,
            now(),
            now()
        )
        ON CONFLICT (rfid_uid) DO UPDATE SET
            scan_count = public.unregistered_medicines.scan_count + 1,
            last_scanned_at = now(),
            compartment = EXCLUDED.compartment;

        -- Return unregistered status to ESP32
        v_result := jsonb_build_object(
            'status', 'unregistered',
            'registered', false,
            'message', 'Invalid ID - Logged for mobile application registration',
            'rfid_uid', UPPER(p_rfid_uid)
        );
    END IF;

    RETURN v_result;
END;
$$;

